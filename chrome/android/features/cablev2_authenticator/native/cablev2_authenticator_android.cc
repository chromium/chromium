// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/timer/elapsed_timer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_registration.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

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

// CableV2MobileEvent enumerates several steps that occur during a caBLEv2
// transaction. Do not change the assigned value since they are used in
// histograms, only append new values. Keep synced with enums.xml.
enum class CableV2MobileEvent {
  kQRRead = 0,
  kServerLink = 1,
  kCloudMessage = 2,
  kUSB = 3,
  kSetup = 4,
  kTunnelServerConnected = 5,
  kHandshakeCompleted = 6,
  kRequestReceived = 7,
  kCTAPError = 8,
  kUnlink = 9,
  kNeedInteractive = 10,
  kInteractionReady = 11,
  kLinkingNotRequested = 12,
  kUSBSuccess = 13,
  kStoppedWhileAwaitingTunnelServerConnection = 14,
  kStoppedWhileAwaitingHandshake = 15,
  kStoppedWhileAwaitingRequest = 16,
  kStoppedWhileAuthenticating = 17,

  kMaxValue = 17,
};

void RecordEvent(CableV2MobileEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.CableV2.MobileEvent", event);
}

// CableV2MobileResult enumerates the outcome of a caBLEv2 transction. Do not
// change the assigned value since they are used in histograms, only append new
// values. Keep synced with enums.xml.
enum class CableV2MobileResult {
  kSuccess = 0,
  kUnexpectedEOF = 1,
  kTunnelServerConnectFailed = 2,
  kHandshakeFailed = 3,
  kDecryptFailure = 4,
  kInvalidCBOR = 5,
  kInvalidCTAP = 6,
  kUnknownCommand = 7,
  kInternalError = 8,
  kInvalidQR = 9,
  kInvalidServerLink = 10,

  kMaxValue = 10,
};

void RecordResult(CableV2MobileResult result) {
  base::UmaHistogramEnumeration("WebAuthentication.CableV2.MobileResult",
                                result);
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

// JavaByteArrayToFixedSpan returns a span that aliases |data|, or |nullopt| if
// the span is not of the correct length. Be aware that the reference for |data|
// must outlive the span.
template <size_t N>
absl::optional<base::span<const uint8_t, N>> JavaByteArrayToFixedSpan(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& data) {
  static_assert(N != 0,
                "Zero case is different from JavaByteArrayToSpan as null "
                "inputs will always be rejected here.");

  if (data.is_null()) {
    return absl::nullopt;
  }

  const size_t data_len = env->GetArrayLength(data);
  if (data_len != N) {
    return absl::nullopt;
  }
  const jbyte* data_bytes = env->GetByteArrayElements(data, /*iscopy=*/nullptr);
  return base::as_bytes(base::make_span<N>(data_bytes, data_len));
}

// GlobalData holds all the state for ongoing security key operations. Since
// there are ultimately only one human user, concurrent requests are not
// supported.
struct GlobalData {
  JNIEnv* env = nullptr;
  // instance_num is incremented for each new |Transaction| created and returned
  // to Java to serve as a "handle". This prevents commands intended for a
  // previous transaction getting applied to a replacement. The zero value is
  // reserved so that functions can still return that to indicate an error.
  jlong instance_num = 1;

  absl::optional<std::array<uint8_t, device::cablev2::kRootSecretSize>>
      root_secret;
  network::mojom::NetworkContext* network_context = nullptr;

  // event_to_record_if_stopped contains an event to record with UMA if the
  // activity is stopped. This is updated as a transaction progresses.
  absl::optional<CableV2MobileEvent> event_to_record_if_stopped;

  // registration is a non-owning pointer to the global |Registration|.
  device::cablev2::authenticator::Registration* registration = nullptr;

  // current_transaction holds the |Transaction| that is currently active.
  std::unique_ptr<device::cablev2::authenticator::Transaction>
      current_transaction;

  // pending_make_credential_callback holds the callback that the
  // |Authenticator| expects to be run once a makeCredential operation has
  // completed.
  absl::optional<
      device::cablev2::authenticator::Platform::MakeCredentialCallback>
      pending_make_credential_callback;
  // pending_get_assertion_callback holds the callback that the
  // |Authenticator| expects to be run once a getAssertion operation has
  // completed.
  absl::optional<device::cablev2::authenticator::Platform::GetAssertionCallback>
      pending_get_assertion_callback;

  // usb_callback holds the callback that receives data from a USB connection.
  absl::optional<
      base::RepeatingCallback<void(absl::optional<base::span<const uint8_t>>)>>
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

  AndroidPlatform(JNIEnv* env,
                  const JavaRef<jobject>& cable_authenticator,
                  bool is_usb)
      : env_(env), cable_authenticator_(cable_authenticator), is_usb_(is_usb) {
    DCHECK(env_->IsInstanceOf(
        cable_authenticator_.obj(),
        org_chromium_chrome_browser_webauth_authenticator_CableAuthenticator_clazz(
            env)));
  }

  ~AndroidPlatform() override = default;

  // Platform:
  void MakeCredential(std::unique_ptr<MakeCredentialParams> params) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_make_credential_callback);
    global_data.pending_make_credential_callback = std::move(params->callback);

    Java_CableAuthenticator_makeCredential(
        env_, cable_authenticator_,
        ConvertUTF8ToJavaString(env_, params->rp_id),
        ToJavaByteArray(env_, params->client_data_hash),
        ToJavaByteArray(env_, params->user_id),
        ToJavaIntArray(env_, params->algorithms),
        ToJavaArrayOfByteArray(env_, params->excluded_cred_ids),
        params->resident_key_required);
  }

  void GetAssertion(std::unique_ptr<GetAssertionParams> params) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_get_assertion_callback);
    global_data.pending_get_assertion_callback = std::move(params->callback);

    Java_CableAuthenticator_getAssertion(
        env_, cable_authenticator_,
        ConvertUTF8ToJavaString(env_, params->rp_id),
        ToJavaByteArray(env_, params->client_data_hash),
        ToJavaArrayOfByteArray(env_, params->allowed_cred_ids));
  }

  void OnStatus(Status status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    LOG(ERROR) << __func__ << " " << static_cast<int>(status);

    CableV2MobileEvent event;
    switch (status) {
      case Status::TUNNEL_SERVER_CONNECT:
        event = CableV2MobileEvent::kTunnelServerConnected;
        tunnel_server_connect_time_.emplace();
        GetGlobalData().event_to_record_if_stopped =
            CableV2MobileEvent::kStoppedWhileAwaitingHandshake;
        break;
      case Status::HANDSHAKE_COMPLETE:
        if (tunnel_server_connect_time_) {
          base::UmaHistogramMediumTimes(
              "WebAuthentication.CableV2.RendezvousTime",
              tunnel_server_connect_time_->Elapsed());
          tunnel_server_connect_time_.reset();
        }
        event = CableV2MobileEvent::kHandshakeCompleted;
        GetGlobalData().event_to_record_if_stopped =
            CableV2MobileEvent::kStoppedWhileAwaitingRequest;
        break;
      case Status::REQUEST_RECEIVED:
        event = CableV2MobileEvent::kRequestReceived;
        GetGlobalData().event_to_record_if_stopped =
            CableV2MobileEvent::kStoppedWhileAuthenticating;
        break;
      case Status::CTAP_ERROR:
        event = CableV2MobileEvent::kCTAPError;
        break;
    }
    RecordEvent(event);

    if (!cable_authenticator_) {
      return;
    }

    Java_CableAuthenticator_onStatus(env_, cable_authenticator_,
                                     static_cast<int>(status));
  }

  void OnCompleted(absl::optional<Error> maybe_error) override {
    LOG(ERROR) << __func__ << " "
               << (maybe_error ? static_cast<int>(*maybe_error) : -1);
    GetGlobalData().event_to_record_if_stopped.reset();

    CableV2MobileResult result = CableV2MobileResult::kSuccess;
    if (maybe_error) {
      switch (*maybe_error) {
        case Error::UNEXPECTED_EOF:
          result = CableV2MobileResult::kUnexpectedEOF;
          break;
        case Error::TUNNEL_SERVER_CONNECT_FAILED:
          result = CableV2MobileResult::kTunnelServerConnectFailed;
          break;
        case Error::HANDSHAKE_FAILED:
          result = CableV2MobileResult::kHandshakeFailed;
          break;
        case Error::DECRYPT_FAILURE:
          result = CableV2MobileResult::kDecryptFailure;
          break;
        case Error::INVALID_CBOR:
          result = CableV2MobileResult::kInvalidCBOR;
          break;
        case Error::INVALID_CTAP:
          result = CableV2MobileResult::kInvalidCTAP;
          break;
        case Error::UNKNOWN_COMMAND:
          result = CableV2MobileResult::kUnknownCommand;
          break;
        case Error::INTERNAL_ERROR:
        case Error::SERVER_LINK_WRONG_LENGTH:
        case Error::SERVER_LINK_NOT_ON_CURVE:
        case Error::NO_SCREENLOCK:
        case Error::NO_BLUETOOTH_PERMISSION:
          result = CableV2MobileResult::kInternalError;
          break;
      }
    }
    RecordResult(result);

    if (is_usb_ && result == CableV2MobileResult::kSuccess) {
      RecordEvent(CableV2MobileEvent::kUSBSuccess);
    }

    // The transaction might fail before interactive mode, thus
    // |cable_authenticator_| may be empty.
    if (cable_authenticator_) {
      const bool ok = !maybe_error.has_value();
      Java_CableAuthenticator_onComplete(
          env_, cable_authenticator_, ok,
          ok ? 0 : static_cast<int>(*maybe_error));
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
  JNIEnv* const env_;
  ScopedJavaGlobalRef<jobject> cable_authenticator_;
  absl::optional<base::ElapsedTimer> tunnel_server_connect_time_;

  // is_usb_ is true if this object was created in order to respond to a client
  // connected over USB.
  const bool is_usb_;

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
  base::RepeatingCallback<void(absl::optional<base::span<const uint8_t>>)>
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
  void OnData(absl::optional<base::span<const uint8_t>> data) {
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

}  // anonymous namespace

// These functions are the entry points for CableAuthenticator.java and
// BLEHandler.java calling into C++.

static void JNI_CableAuthenticator_Setup(
    JNIEnv* env,
    jlong registration_long,
    jlong network_context_long,
    const JavaParamRef<jbyteArray>& secret) {
  GlobalData& global_data = GetGlobalData();

  // The root_secret may not be provided when triggered for server-link. It
  // won't be used in that case either, but we need to be able to grab it if
  // setup() is called called for a different type of exchange.
  base::span<const uint8_t> root_secret = JavaByteArrayToSpan(env, secret);
  if (!root_secret.empty() && !global_data.root_secret) {
    global_data.root_secret.emplace();
    CHECK_EQ(global_data.root_secret->size(), root_secret.size());
    memcpy(global_data.root_secret->data(), root_secret.data(),
           global_data.root_secret->size());
  }

  // If starting a new transaction, don't record anything if stopped.
  global_data.event_to_record_if_stopped.reset();

  // This function can be called multiple times and must be idempotent. The
  // |env| member of |global_data| is used to flag whether setup has
  // already occurred.
  if (global_data.env) {
    return;
  }

  RecordEvent(CableV2MobileEvent::kSetup);
  global_data.env = env;

  if (base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    // If kWebAuthPhoneSupport isn't enabled then QR scanning isn't enabled and
    // thus no linking messages will be sent. Thus there's no point in burdening
    // FCM with registrations.
    static_assert(sizeof(jlong) >= sizeof(void*), "");
    global_data.registration =
        reinterpret_cast<device::cablev2::authenticator::Registration*>(
            registration_long);
    global_data.registration->PrepareContactID();
  }

  global_data.network_context =
      reinterpret_cast<network::mojom::NetworkContext*>(network_context_long);
}

static jlong JNI_CableAuthenticator_StartUSB(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jobject>& usb_device) {
  RecordEvent(CableV2MobileEvent::kUSB);

  GlobalData& global_data = GetGlobalData();

  auto transport = std::make_unique<USBTransport>(
      env, ScopedJavaGlobalRef<jobject>(usb_device));
  DCHECK(!global_data.usb_callback);
  global_data.usb_callback = transport->GetCallback();

  global_data.current_transaction =
      device::cablev2::authenticator::TransactWithPlaintextTransport(
          std::make_unique<AndroidPlatform>(env, cable_authenticator,
                                            /*is_usb=*/true),
          std::unique_ptr<device::cablev2::authenticator::Transport>(
              transport.release()));

  return ++global_data.instance_num;
}

static jlong JNI_CableAuthenticator_StartQR(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jstring>& authenticator_name,
    const JavaParamRef<jstring>& qr_url,
    jboolean link) {
  RecordEvent(CableV2MobileEvent::kQRRead);

  GlobalData& global_data = GetGlobalData();
  const std::string& qr_string = ConvertJavaStringToUTF8(qr_url);
  absl::optional<device::cablev2::qr::Components> decoded_qr(
      device::cablev2::qr::Parse(qr_string));
  if (!decoded_qr) {
    FIDO_LOG(ERROR) << "Failed to decode QR: " << qr_string;
    RecordResult(CableV2MobileResult::kInvalidQR);
    return 0;
  }

  if (!link) {
    RecordEvent(CableV2MobileEvent::kLinkingNotRequested);
  }

  global_data.event_to_record_if_stopped =
      CableV2MobileEvent::kStoppedWhileAwaitingTunnelServerConnection;
  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          std::make_unique<AndroidPlatform>(env, cable_authenticator,
                                            /*is_usb=*/false),
          global_data.network_context, *global_data.root_secret,
          ConvertJavaStringToUTF8(authenticator_name), decoded_qr->secret,
          decoded_qr->peer_identity,
          link ? global_data.registration->contact_id() : absl::nullopt);

  return ++global_data.instance_num;
}

static jlong JNI_CableAuthenticator_StartServerLink(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jbyteArray>& server_link_data_java) {
  RecordEvent(CableV2MobileEvent::kServerLink);

  constexpr size_t kDataSize =
      device::kP256X962Length + device::cablev2::kQRSecretSize;
  const absl::optional<base::span<const uint8_t, kDataSize>> server_link_data =
      JavaByteArrayToFixedSpan<kDataSize>(env, server_link_data_java);
  // validateServerLinkData should have been called to check this already.
  CHECK(server_link_data);

  // Sending pairing information is disabled when doing a server-linked
  // connection, thus the root secret and authenticator name will not be used.
  std::array<uint8_t, device::cablev2::kRootSecretSize> dummy_root_secret = {0};
  std::string dummy_authenticator_name = "";
  GlobalData& global_data = GetGlobalData();
  global_data.event_to_record_if_stopped =
      CableV2MobileEvent::kStoppedWhileAwaitingTunnelServerConnection;
  global_data
      .current_transaction = device::cablev2::authenticator::TransactFromQRCode(
      std::make_unique<AndroidPlatform>(env, cable_authenticator,
                                        /*is_usb=*/false),
      global_data.network_context, dummy_root_secret, dummy_authenticator_name,
      server_link_data
          ->subspan<device::kP256X962Length, device::cablev2::kQRSecretSize>(),
      server_link_data->subspan<0, device::kP256X962Length>(), absl::nullopt);

  return ++global_data.instance_num;
}

static jlong JNI_CableAuthenticator_StartCloudMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jbyteArray>& serialized_event) {
  RecordEvent(CableV2MobileEvent::kCloudMessage);

  auto event =
      device::cablev2::authenticator::Registration::Event::FromSerialized(
          JavaByteArrayToSpan(env, serialized_event));
  if (!event) {
    LOG(ERROR) << "Failed to parse event";
    return 0;
  }

  DCHECK((event->source ==
          device::cablev2::authenticator::Registration::Type::LINKING) ==
         event->contact_id.has_value());

  GlobalData& global_data = GetGlobalData();
  // There is deliberately no check for |!global_data.current_transaction|
  // because multiple Cloud messages may come in from different paired devices.
  // Only the most recent is processed.
  global_data.event_to_record_if_stopped =
      CableV2MobileEvent::kStoppedWhileAwaitingTunnelServerConnection;
  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromFCM(
          std::make_unique<AndroidPlatform>(env, cable_authenticator,
                                            /*is_usb=*/false),
          global_data.network_context, *global_data.root_secret,
          event->routing_id, event->tunnel_id, event->pairing_id,
          event->client_nonce, event->contact_id);

  return ++global_data.instance_num;
}

static void JNI_CableAuthenticator_Unlink(JNIEnv* env) {
  RecordEvent(CableV2MobileEvent::kUnlink);

  GlobalData& global_data = GetGlobalData();
  global_data.registration->RotateContactID();
}

static void JNI_CableAuthenticator_Stop(JNIEnv* env, jlong instance_num) {
  GlobalData& global_data = GetGlobalData();
  if (global_data.instance_num == instance_num) {
    ResetGlobalData();
  }
}

static int JNI_CableAuthenticator_ValidateServerLinkData(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& jdata) {
  base::span<const uint8_t> data = JavaByteArrayToSpan(env, jdata);
  if (data.size() != device::kP256X962Length + device::cablev2::kQRSecretSize) {
    RecordResult(CableV2MobileResult::kInvalidServerLink);
    return static_cast<int>(device::cablev2::authenticator::Platform::Error::
                                SERVER_LINK_WRONG_LENGTH);
  }

  base::span<const uint8_t> x962 = data.subspan(0, device::kP256X962Length);
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), x962.data(), x962.size(),
                          /*ctx=*/nullptr)) {
    RecordResult(CableV2MobileResult::kInvalidServerLink);
    return static_cast<int>(device::cablev2::authenticator::Platform::Error::
                                SERVER_LINK_NOT_ON_CURVE);
  }

  return 0;
}

static void JNI_CableAuthenticator_OnActivityStop(JNIEnv* env,
                                                  jlong instance_num) {
  GlobalData& global_data = GetGlobalData();
  if (global_data.event_to_record_if_stopped &&
      global_data.instance_num == instance_num) {
    RecordEvent(*global_data.event_to_record_if_stopped);
    global_data.event_to_record_if_stopped.reset();
  }
}

static void JNI_CableAuthenticator_OnAuthenticatorAttestationResponse(
    JNIEnv* env,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jattestation_object) {
  GlobalData& global_data = GetGlobalData();

  if (!global_data.pending_make_credential_callback) {
    return;
  }
  auto callback = std::move(*global_data.pending_make_credential_callback);
  global_data.pending_make_credential_callback.reset();

  std::move(callback).Run(ctap_status,
                          JavaByteArrayToSpan(env, jattestation_object));
}

static void JNI_CableAuthenticator_OnAuthenticatorAssertionResponse(
    JNIEnv* env,
    jint ctap_status,
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
    global_data.usb_callback->Run(absl::nullopt);
  } else {
    global_data.usb_callback->Run(JavaByteArrayToSpan(env, usb_data));
  }
}
