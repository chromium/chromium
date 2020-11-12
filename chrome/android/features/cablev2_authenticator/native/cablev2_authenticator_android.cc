// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_registration.h"
#include "device/fido/fido_parsing_utils.h"

// These "headers" actually contain several function definitions and thus can
// only be included once across Chromium.
#include "chrome/android/features/cablev2_authenticator/jni_headers/BLEAdvert_jni.h"
#include "chrome/android/features/cablev2_authenticator/jni_headers/CableAuthenticator_jni.h"
#include "chrome/android/features/cablev2_authenticator/jni_headers/USBHandler_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaByteArrayToByteVector;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfByteArray;
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;

namespace {

// ParseState converts the bytes stored by Java into a root secret value.
base::Optional<std::array<uint8_t, device::cablev2::kRootSecretSize>>
ParseState(base::span<const uint8_t> state_bytes) {
  base::Optional<cbor::Value> state = cbor::Reader::Read(state_bytes);
  if (!state || !state->is_map()) {
    return base::nullopt;
  }

  const cbor::Value::MapValue& state_map(state->GetMap());
  std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret;
  if (!device::fido_parsing_utils::CopyCBORBytestring(&root_secret, state_map,
                                                      1)) {
    return base::nullopt;
  }

  return root_secret;
}

// NewState creates a fresh root secret and its serialisation.
std::pair<std::array<uint8_t, device::cablev2::kRootSecretSize>,
          std::vector<uint8_t>>
NewState() {
  std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret;
  crypto::RandBytes(root_secret);

  cbor::Value::MapValue map;
  map.emplace(1, cbor::Value(root_secret));

  base::Optional<std::vector<uint8_t>> bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  CHECK(bytes.has_value());

  return std::make_tuple(root_secret, std::move(*bytes));
}

// JavaByteArrayToSpan returns a span that aliases |data|. Be aware that the
// reference for |data| must outlive the span.
base::span<const uint8_t> JavaByteArrayToSpan(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& data) {
  if (data.is_null()) {
    return base::span<const uint8_t>();
  }

  const size_t data_len = env->GetArrayLength(data);
  const jbyte* data_bytes = env->GetByteArrayElements(data, /*iscopy=*/nullptr);
  return base::as_bytes(base::make_span(data_bytes, data_len));
}

// GlobalData holds all the state for ongoing security key operations. Since
// there are ultimately only one human user, concurrent requests are not
// supported.
struct GlobalData {
  JNIEnv* env = nullptr;
  std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret;
  network::mojom::NetworkContext* network_context = nullptr;

  // registration is a non-owning pointer to the global |Registration|.
  device::cablev2::authenticator::Registration* registration = nullptr;

  // activity_class_name is the name of a Java class that should be the target
  // of any notifications shown.
  std::string activity_class_name;

  // interaction_ready_callback is called when the |CableAuthenticatorUI|
  // Fragment comes into the foreground after a request to do so from the
  // |AndroidPlatform|. The |cable_authenticator| parameter is a reference to
  // the newly created |CableAuthenticator| Java object, which can be used to
  // start interactive actions such a getting an assertion.
  base::OnceCallback<void(ScopedJavaGlobalRef<jobject> cable_authenticator)>
      interaction_ready_callback;

  // current_transaction holds the |Transaction| that is currently active.
  std::unique_ptr<device::cablev2::authenticator::Transaction>
      current_transaction;

  // pending_make_credential_callback holds the callback that the
  // |Authenticator| expects to be run once a makeCredential operation has
  // completed.
  base::Optional<
      device::cablev2::authenticator::Platform::MakeCredentialCallback>
      pending_make_credential_callback;
  // pending_get_assertion_callback holds the callback that the
  // |Authenticator| expects to be run once a getAssertion operation has
  // completed.
  base::Optional<device::cablev2::authenticator::Platform::GetAssertionCallback>
      pending_get_assertion_callback;

  // usb_callback holds the callback that receives data from a USB connection.
  base::Optional<
      base::RepeatingCallback<void(base::Optional<base::span<const uint8_t>>)>>
      usb_callback;
};

// GetGlobalData returns a pointer to the unique |GlobalData| for the address
// space.
GlobalData& GetGlobalData() {
  static base::NoDestructor<GlobalData> global_data;
  return *global_data;
}

void ResetGlobalData() {
  GlobalData& global_data = GetGlobalData();
  global_data.current_transaction.reset();
  global_data.pending_make_credential_callback.reset();
  global_data.pending_get_assertion_callback.reset();
  global_data.usb_callback.reset();
}

// AndroidBLEAdvert wraps a Java |BLEAdvert| object so that
// |authenticator::Platform| can hold it.
class AndroidBLEAdvert
    : public device::cablev2::authenticator::Platform::BLEAdvert {
 public:
  AndroidBLEAdvert(JNIEnv* env, ScopedJavaGlobalRef<jobject> advert)
      : env_(env), advert_(std::move(advert)) {
    DCHECK(env_->IsInstanceOf(
        advert_.obj(),
        org_chromium_chrome_browser_webauth_authenticator_BLEAdvert_clazz(
            env)));
  }

  ~AndroidBLEAdvert() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    Java_BLEAdvert_close(env_, advert_);
  }

 private:
  JNIEnv* const env_;
  const ScopedJavaGlobalRef<jobject> advert_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// AndroidPlatform implements |authenticator::Platform| using the GMSCore
// implementation of FIDO operations.
class AndroidPlatform : public device::cablev2::authenticator::Platform {
 public:
  typedef base::OnceCallback<void(ScopedJavaGlobalRef<jobject>)>
      InteractionReadyCallback;
  typedef base::OnceCallback<void(InteractionReadyCallback)>
      InteractionNeededCallback;

  AndroidPlatform(JNIEnv* env, const JavaRef<jobject>& cable_authenticator)
      : env_(env), cable_authenticator_(cable_authenticator) {
    DCHECK(env_->IsInstanceOf(
        cable_authenticator_.obj(),
        org_chromium_chrome_browser_webauth_authenticator_CableAuthenticator_clazz(
            env)));
  }

  // This constructor may be used when a |CableAuthenticator| reference is
  // unavailable because the code is running in the background. Sending BLE
  // adverts is possible in this state, but performing anything that requires
  // interaction (i.e. making a credential or getting an assertion) is not. If
  // anything requiring interaction is requested then the given callback will be
  // called. It must arrange for a foreground |CableAuthenticator| to start and,
  // when ready, call the passed callback with a reference to it. The pending
  // action will then complete as normal.
  AndroidPlatform(JNIEnv* env,
                  InteractionNeededCallback interaction_needed_callback)
      : env_(env),
        interaction_needed_callback_(std::move(interaction_needed_callback)) {}

  ~AndroidPlatform() override {
    if (notification_showing_) {
      Java_CableAuthenticator_dropNotification(env_);
    }
  }

  // Platform:
  void MakeCredential(std::unique_ptr<MakeCredentialParams> params) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (cable_authenticator_) {
      CallMakeCredential(std::move(params));
      return;
    }

    DCHECK(!pending_make_credential_);
    pending_make_credential_ = std::move(params);
    NeedInteractive();
  }

  void GetAssertion(std::unique_ptr<GetAssertionParams> params) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (cable_authenticator_) {
      CallGetAssertion(std::move(params));
      return;
    }

    DCHECK(!pending_get_assertion_);
    pending_get_assertion_ = std::move(params);
    NeedInteractive();
  }

  void OnStatus(Status status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    LOG(ERROR) << __func__ << " " << static_cast<int>(status);

    if (!cable_authenticator_) {
      return;
    }

    Java_CableAuthenticator_onStatus(env_, cable_authenticator_,
                                     static_cast<int>(status));
  }

  void OnCompleted(base::Optional<Error> maybe_error) override {
    LOG(ERROR) << __func__ << " "
               << (maybe_error ? static_cast<int>(*maybe_error) : -1);

    // The transaction might fail before interactive mode, thus
    // |cable_authenticator_| may be empty.
    if (cable_authenticator_) {
      Java_CableAuthenticator_onComplete(env_, cable_authenticator_);
    }
    // ResetGlobalData will delete the |Transaction|, which will delete this
    // object. Thus nothing else can be done after this call.
    ResetGlobalData();
  }

  std::unique_ptr<BLEAdvert> SendBLEAdvert(
      base::span<const uint8_t, device::cablev2::kAdvertSize> payload)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return std::make_unique<AndroidBLEAdvert>(
        env_, ScopedJavaGlobalRef<jobject>(Java_CableAuthenticator_newBLEAdvert(
                  env_, ToJavaByteArray(env_, payload))));
  }

 private:
  void CallMakeCredential(std::unique_ptr<MakeCredentialParams> params) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_make_credential_callback);
    global_data.pending_make_credential_callback = std::move(params->callback);

    Java_CableAuthenticator_makeCredential(
        env_, cable_authenticator_,
        ConvertUTF8ToJavaString(env_, params->origin),
        ConvertUTF8ToJavaString(env_, params->rp_id),
        ToJavaByteArray(env_, params->challenge),
        ToJavaByteArray(env_, params->user_id),
        ToJavaIntArray(env_, params->algorithms),
        ToJavaArrayOfByteArray(env_, params->excluded_cred_ids),
        params->resident_key_required);
  }

  void CallGetAssertion(std::unique_ptr<GetAssertionParams> params) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_get_assertion_callback);
    global_data.pending_get_assertion_callback = std::move(params->callback);

    Java_CableAuthenticator_getAssertion(
        env_, cable_authenticator_,
        ConvertUTF8ToJavaString(env_, params->origin),
        ConvertUTF8ToJavaString(env_, params->rp_id),
        ToJavaByteArray(env_, params->challenge),
        ToJavaArrayOfByteArray(env_, params->allowed_cred_ids));
  }

  // NeedInteractive is called when this object is operating in the background,
  // but we need to trigger interactive mode in order to show UI.
  void NeedInteractive() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    notification_showing_ = true;
    std::move(interaction_needed_callback_)
        .Run(base::BindOnce(&AndroidPlatform::OnInteractionReady,
                            weak_factory_.GetWeakPtr()));
  }

  // OnInteractionReady is called when the caBLE Activity is running in the
  // foreground after a |NeedInteractive| call.
  void OnInteractionReady(ScopedJavaGlobalRef<jobject> cable_authenticator) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!cable_authenticator_);
    DCHECK(notification_showing_);
    DCHECK(env_->IsInstanceOf(
        cable_authenticator.obj(),
        org_chromium_chrome_browser_webauth_authenticator_CableAuthenticator_clazz(
            env_)));
    cable_authenticator_ = std::move(cable_authenticator);
    notification_showing_ = false;

    DCHECK(static_cast<bool>(pending_make_credential_) ^
           static_cast<bool>(pending_get_assertion_));
    if (pending_make_credential_) {
      CallMakeCredential(std::move(pending_make_credential_));
    } else {
      CallGetAssertion(std::move(pending_get_assertion_));
    }
  }

  JNIEnv* const env_;
  bool notification_showing_ = false;
  ScopedJavaGlobalRef<jobject> cable_authenticator_;
  std::unique_ptr<MakeCredentialParams> pending_make_credential_;
  std::unique_ptr<GetAssertionParams> pending_get_assertion_;
  InteractionNeededCallback interaction_needed_callback_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AndroidPlatform> weak_factory_{this};
};

// USBTransport wraps the Java |USBHandler| object so that
// |authenticator::Platform| can use it as a transport.
class USBTransport : public device::cablev2::authenticator::Transport {
 public:
  USBTransport(JNIEnv* env, ScopedJavaGlobalRef<jobject> usb_device)
      : env_(env), usb_device_(std::move(usb_device)) {
    DCHECK(env_->IsInstanceOf(
        usb_device_.obj(),
        org_chromium_chrome_browser_webauth_authenticator_USBHandler_clazz(
            env)));
  }

  ~USBTransport() override { Java_USBHandler_close(env_, usb_device_); }

  // GetCallback returns callback which will be called repeatedly with data from
  // the USB connection, forwarded via the Java code.
  base::RepeatingCallback<void(base::Optional<base::span<const uint8_t>>)>
  GetCallback() {
    return base::BindRepeating(&USBTransport::OnData,
                               weak_factory_.GetWeakPtr());
  }

  // Transport:
  void StartReading(
      base::RepeatingCallback<void(Update)> update_callback) override {
    callback_ = update_callback;
    Java_USBHandler_startReading(env_, usb_device_);
  }

  void Write(std::vector<uint8_t> data) override {
    Java_USBHandler_write(env_, usb_device_, ToJavaByteArray(env_, data));
  }

 private:
  void OnData(base::Optional<base::span<const uint8_t>> data) {
    if (!data) {
      callback_.Run(Disconnected::kDisconnected);
    } else {
      callback_.Run(device::fido_parsing_utils::Materialize(*data));
    }
  }

  JNIEnv* const env_;
  const ScopedJavaGlobalRef<jobject> usb_device_;
  base::RepeatingCallback<void(Update)> callback_;
  base::WeakPtrFactory<USBTransport> weak_factory_{this};
};

// OnNeedInteractive is called by |AndroidPlatform| when it needs to move from
// the background to the foreground in order to show UI.
void OnNeedInteractive(AndroidPlatform::InteractionReadyCallback callback) {
  GlobalData& global_data = GetGlobalData();
  global_data.interaction_ready_callback = std::move(callback);
  Java_CableAuthenticator_showNotification(
      global_data.env, ConvertUTF8ToJavaString(
                           global_data.env, global_data.activity_class_name));
}

}  // anonymous namespace

// These functions are the entry points for CableAuthenticator.java and
// BLEHandler.java calling into C++.

static ScopedJavaLocalRef<jbyteArray> JNI_CableAuthenticator_Setup(
    JNIEnv* env,
    jlong registration_long,
    const JavaParamRef<jstring>& activity_class_name,
    jlong network_context_long,
    const JavaParamRef<jbyteArray>& state_bytes) {
  std::vector<uint8_t> serialized_state;

  GlobalData& global_data = GetGlobalData();
  // This function can be called multiple times and must be idempotent. The
  // |registration| member of |global_data| is used to flag whether setup has
  // already occurred.
  if (global_data.registration) {
    // If setup has already occurred then an empty byte[] is returned to
    // indicate that no update is needed.
    return ToJavaByteArray(env, serialized_state);
  }

  base::Optional<std::array<uint8_t, device::cablev2::kRootSecretSize>>
      maybe_root_secret = ParseState(JavaByteArrayToSpan(env, state_bytes));

  if (!maybe_root_secret) {
    std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret;
    std::tie(root_secret, serialized_state) = NewState();
    maybe_root_secret = root_secret;
  }
  global_data.root_secret = *maybe_root_secret;

  global_data.env = env;
  global_data.activity_class_name =
      ConvertJavaStringToUTF8(activity_class_name);

  static_assert(sizeof(jlong) >= sizeof(void*), "");
  global_data.registration =
      reinterpret_cast<device::cablev2::authenticator::Registration*>(
          registration_long);
  global_data.registration->PrepareContactID();

  global_data.network_context =
      reinterpret_cast<network::mojom::NetworkContext*>(network_context_long);

  return ToJavaByteArray(env, serialized_state);
}

static void JNI_CableAuthenticator_StartUSB(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jobject>& usb_device) {
  GlobalData& global_data = GetGlobalData();

  auto transport = std::make_unique<USBTransport>(
      env, ScopedJavaGlobalRef<jobject>(usb_device));
  DCHECK(!global_data.usb_callback);
  global_data.usb_callback = transport->GetCallback();

  global_data.current_transaction =
      device::cablev2::authenticator::TransactWithPlaintextTransport(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          std::unique_ptr<device::cablev2::authenticator::Transport>(
              transport.release()));
}

static jboolean JNI_CableAuthenticator_StartQR(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jstring>& authenticator_name,
    const JavaParamRef<jstring>& qr_url) {
  GlobalData& global_data = GetGlobalData();
  const std::string& qr_string = ConvertJavaStringToUTF8(qr_url);
  base::Optional<device::cablev2::qr::Components> decoded_qr(
      device::cablev2::qr::Parse(qr_string));
  if (!decoded_qr) {
    FIDO_LOG(ERROR) << "Failed to decode QR: " << qr_string;
    return false;
  }

  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          global_data.network_context, global_data.root_secret,
          ConvertJavaStringToUTF8(authenticator_name), decoded_qr->secret,
          decoded_qr->peer_identity, global_data.registration->contact_id());

  return true;
}

static ScopedJavaLocalRef<jbyteArray> JNI_CableAuthenticator_Unlink(
    JNIEnv* env) {
  std::vector<uint8_t> serialized_state;
  std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret;
  std::tie(root_secret, serialized_state) = NewState();

  GlobalData& global_data = GetGlobalData();
  global_data.root_secret = root_secret;
  global_data.registration->RotateContactID();

  return ToJavaByteArray(env, serialized_state);
}

static void JNI_CableAuthenticator_OnInteractionReady(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator) {
  GlobalData& global_data = GetGlobalData();
  std::move(global_data.interaction_ready_callback)
      .Run(ScopedJavaGlobalRef<jobject>(cable_authenticator));
}

static void JNI_CableAuthenticator_Stop(JNIEnv* env) {
  ResetGlobalData();
}

static void JNI_CableAuthenticator_OnCloudMessage(JNIEnv* env,
                                                  jlong event_long) {
  static_assert(sizeof(jlong) >= sizeof(void*), "");
  std::unique_ptr<device::cablev2::authenticator::Registration::Event> event(
      reinterpret_cast<device::cablev2::authenticator::Registration::Event*>(
          event_long));

  GlobalData& global_data = GetGlobalData();

  // TODO(agl): should enable Bluetooth here as needed.

  // There is deliberately no check for |!global_data.current_transaction|
  // because multiple Cloud messages may come in from different paired devices.
  // Only the most recent is processed.
  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromFCM(
          std::make_unique<AndroidPlatform>(env,
                                            base::BindOnce(&OnNeedInteractive)),
          global_data.network_context, global_data.root_secret,
          event->routing_id, event->tunnel_id, event->pairing_id,
          event->client_nonce);
}

static void JNI_CableAuthenticator_OnAuthenticatorAttestationResponse(
    JNIEnv* env,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jclient_data_json,
    const JavaParamRef<jbyteArray>& jattestation_object) {
  GlobalData& global_data = GetGlobalData();

  if (!global_data.pending_make_credential_callback) {
    return;
  }
  auto callback = std::move(*global_data.pending_make_credential_callback);
  global_data.pending_make_credential_callback.reset();

  std::move(callback).Run(ctap_status,
                          JavaByteArrayToSpan(env, jclient_data_json),
                          JavaByteArrayToSpan(env, jattestation_object));
}

static void JNI_CableAuthenticator_OnAuthenticatorAssertionResponse(
    JNIEnv* env,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jclient_data_json,
    const JavaParamRef<jbyteArray>& jcredential_id,
    const JavaParamRef<jbyteArray>& jauthenticator_data,
    const JavaParamRef<jbyteArray>& jsignature) {
  GlobalData& global_data = GetGlobalData();

  if (!global_data.pending_get_assertion_callback) {
    return;
  }
  auto callback = std::move(*global_data.pending_get_assertion_callback);
  global_data.pending_get_assertion_callback.reset();

  std::move(callback).Run(ctap_status,
                          JavaByteArrayToSpan(env, jclient_data_json),
                          JavaByteArrayToSpan(env, jcredential_id),
                          JavaByteArrayToSpan(env, jauthenticator_data),
                          JavaByteArrayToSpan(env, jsignature));
}

static void JNI_USBHandler_OnUSBData(JNIEnv* env,
                                     const JavaParamRef<jbyteArray>& usb_data) {
  GlobalData& global_data = GetGlobalData();
  if (!global_data.usb_callback) {
    return;
  }

  if (!usb_data) {
    global_data.usb_callback->Run(base::nullopt);
  } else {
    global_data.usb_callback->Run(JavaByteArrayToSpan(env, usb_data));
  }
}
