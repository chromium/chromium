// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/base64url.h"
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
#include "third_party/boringssl/src/include/openssl/ec_key.h"
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

// DecodeQR represents the values in a scanned QR code.
struct DecodedQR {
  std::array<uint8_t, 16> secret;
  std::array<uint8_t, device::kP256X962Length> peer_identity;
};

// DecompressPublicKey converts a compressed public key (from a scanned QR
// code) into a standard, uncompressed one.
base::Optional<std::array<uint8_t, device::kP256X962Length>>
DecompressPublicKey(
    base::span<const uint8_t, device::cablev2::kCompressedPublicKeySize>
        compressed_public_key) {
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), compressed_public_key.data(),
                          compressed_public_key.size(), /*ctx=*/nullptr)) {
    return base::nullopt;
  }
  std::array<uint8_t, device::kP256X962Length> ret;
  CHECK_EQ(
      ret.size(),
      EC_POINT_point2oct(p256.get(), point.get(), POINT_CONVERSION_UNCOMPRESSED,
                         ret.data(), ret.size(), /*ctx=*/nullptr));
  return ret;
}

// DecodeQR converts the textual form of a scanned QR code into a |DecodedQR|.
base::Optional<DecodedQR> DecodeQR(const std::string& qr_url) {
  static const char kPrefix[] = "fido://c1/";
  // The scanning code should have filtered out any unrelated URLs.
  if (qr_url.find(kPrefix) != 0) {
    NOTREACHED();
    return base::nullopt;
  }

  base::StringPiece qr_url_base64(qr_url);
  qr_url_base64 = qr_url_base64.substr(sizeof(kPrefix) - 1);
  std::string qr_data_str;
  if (!base::Base64UrlDecode(qr_url_base64,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &qr_data_str) ||
      qr_data_str.size() != device::cablev2::kQRDataSize) {
    FIDO_LOG(ERROR) << "QR decoding failed: " << qr_url;
    return base::nullopt;
  }

  base::span<const uint8_t, device::cablev2::kQRDataSize> qr_data(
      reinterpret_cast<const uint8_t*>(qr_data_str.data()), qr_data_str.size());

  static_assert(EXTENT(qr_data) == device::cablev2::kCompressedPublicKeySize +
                                       device::cablev2::kQRSecretSize,
                "");
  base::span<const uint8_t, device::cablev2::kCompressedPublicKeySize>
      compressed_public_key(qr_data.data(),
                            device::cablev2::kCompressedPublicKeySize);
  auto qr_secret = qr_data.subspan(device::cablev2::kCompressedPublicKeySize);

  DecodedQR ret;
  DCHECK_EQ(qr_secret.size(), ret.secret.size());
  std::copy(qr_secret.begin(), qr_secret.end(), ret.secret.begin());

  base::Optional<std::array<uint8_t, device::kP256X962Length>> peer_identity =
      DecompressPublicKey(compressed_public_key);
  if (!peer_identity) {
    FIDO_LOG(ERROR) << "Invalid compressed public key in QR data";
    return base::nullopt;
  }

  ret.peer_identity = *peer_identity;
  return ret;
}

// JavaByteArrayToSpan returns a span that aliases |data|. Be aware that the
// reference for |data| must outlive the span.
base::span<const uint8_t> JavaByteArrayToSpan(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& data) {
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

  // registration owns the object that handles cloud messages.
  std::unique_ptr<device::cablev2::authenticator::Registration> registration;
  // activity_class_name is the name of a Java class that should be the target
  // of any notifications shown.
  std::string activity_class_name;
  // fragment_class_name is the name of a Java class that is passed to the
  // |activity_class_name| when a notification is activated.
  std::string fragment_class_name;

  // last_event stores the last cloud message received. Android strongly
  // discourages keeping state inside the notification itself. Thus
  // notifications are content-less and the state is kept here.
  std::unique_ptr<device::cablev2::authenticator::Registration::Event>
      last_event;

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

// OnContactEvent is called when the tunnel service alerts us to a tunnel
// request from a paired device.
void OnContactEvent(
    std::unique_ptr<device::cablev2::authenticator::Registration::Event>
        event) {
  GlobalData& global_data = GetGlobalData();

  global_data.last_event = std::move(event);

  Java_CableAuthenticator_showNotification(
      global_data.env,
      ConvertUTF8ToJavaString(global_data.env, global_data.activity_class_name),
      ConvertUTF8ToJavaString(global_data.env,
                              global_data.fragment_class_name));
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
        env_->FindClass(
            "org/chromium/chrome/browser/webauth/authenticator/BLEAdvert")));
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
  AndroidPlatform(JNIEnv* env, const JavaParamRef<jobject>& cable_authenticator)
      : env_(env), cable_authenticator_(cable_authenticator) {
    DCHECK(env_->IsInstanceOf(
        cable_authenticator_.obj(),
        env_->FindClass("org/chromium/chrome/browser/webauth/authenticator/"
                        "CableAuthenticator")));
  }

  // Platform:
  void MakeCredential(
      const std::string& origin,
      const std::string& rp_id,
      base::span<const uint8_t> challenge,
      base::span<const uint8_t> user_id,
      base::span<const int> algorithms,
      base::span<const std::vector<uint8_t>> excluded_cred_ids,
      bool resident_key_required,
      device::cablev2::authenticator::Platform::MakeCredentialCallback callback)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_make_credential_callback);
    global_data.pending_make_credential_callback = std::move(callback);

    Java_CableAuthenticator_makeCredential(
        env_, cable_authenticator_, ConvertUTF8ToJavaString(env_, origin),
        ConvertUTF8ToJavaString(env_, rp_id), ToJavaByteArray(env_, challenge),
        ToJavaByteArray(env_, user_id), ToJavaIntArray(env_, algorithms),
        ToJavaArrayOfByteArray(env_, excluded_cred_ids), resident_key_required);
  }

  void GetAssertion(
      const std::string& origin,
      const std::string& rp_id,
      base::span<const uint8_t> challenge,
      base::span<const std::vector<uint8_t>> allowed_cred_ids,
      device::cablev2::authenticator::Platform::GetAssertionCallback callback)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_get_assertion_callback);
    global_data.pending_get_assertion_callback = std::move(callback);

    Java_CableAuthenticator_getAssertion(
        env_, cable_authenticator_, ConvertUTF8ToJavaString(env_, origin),
        ConvertUTF8ToJavaString(env_, rp_id), ToJavaByteArray(env_, challenge),
        ToJavaArrayOfByteArray(env_, allowed_cred_ids));
  }

  std::unique_ptr<BLEAdvert> SendBLEAdvert(
      base::span<uint8_t, 16> payload) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return std::make_unique<AndroidBLEAdvert>(
        env_, ScopedJavaGlobalRef<jobject>(Java_CableAuthenticator_newBLEAdvert(
                  env_, ToJavaByteArray(env_, payload))));
  }

 private:
  JNIEnv* const env_;
  const ScopedJavaGlobalRef<jobject> cable_authenticator_;
  SEQUENCE_CHECKER(sequence_checker_);
};

void ResetGlobalData() {
  GlobalData& global_data = GetGlobalData();
  global_data.current_transaction.reset();
  global_data.pending_make_credential_callback.reset();
  global_data.pending_get_assertion_callback.reset();
  global_data.usb_callback.reset();
  global_data.last_event.reset();
}

// TransactionComplete is called by |authenticator::Platform| whenever a
// transaction has completed.
void TransactionComplete(JNIEnv* env,
                         ScopedJavaGlobalRef<jobject> cable_authenticator) {
  ResetGlobalData();
  Java_CableAuthenticator_onComplete(env, cable_authenticator);
}

// USBTransport wraps the Java |USBHandler| object so that
// |authenticator::Platform| can use it as a transport.
class USBTransport : public device::cablev2::authenticator::Transport {
 public:
  USBTransport(JNIEnv* env, ScopedJavaGlobalRef<jobject> usb_device)
      : env_(env), usb_device_(std::move(usb_device)) {
    DCHECK(env_->IsInstanceOf(
        usb_device_.obj(),
        env_->FindClass(
            "org/chromium/chrome/browser/webauth/authenticator/USBHandler")));
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
      base::RepeatingCallback<void(base::Optional<std::vector<uint8_t>>)>
          read_callback) override {
    callback_ = read_callback;
    Java_USBHandler_startReading(env_, usb_device_);
  }

  void Write(std::vector<uint8_t> data) override {
    Java_USBHandler_write(env_, usb_device_, ToJavaByteArray(env_, data));
  }

 private:
  void OnData(base::Optional<base::span<const uint8_t>> data) {
    if (!data) {
      callback_.Run(base::nullopt);
    } else {
      callback_.Run(device::fido_parsing_utils::Materialize(*data));
    }
  }

  JNIEnv* const env_;
  const ScopedJavaGlobalRef<jobject> usb_device_;
  base::RepeatingCallback<void(base::Optional<std::vector<uint8_t>>)> callback_;
  base::WeakPtrFactory<USBTransport> weak_factory_{this};
};

}  // anonymous namespace

// These functions are the entry points for CableAuthenticator.java and
// BLEHandler.java calling into C++.

static ScopedJavaLocalRef<jbyteArray> JNI_CableAuthenticator_Setup(
    JNIEnv* env,
    jlong instance_id_driver_long,
    const JavaParamRef<jstring>& activity_class_name,
    const JavaParamRef<jstring>& fragment_class_name,
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
  global_data.fragment_class_name =
      ConvertJavaStringToUTF8(fragment_class_name);

  static_assert(sizeof(jlong) >= sizeof(void*), "");
  auto* instance_id_driver =
      reinterpret_cast<instance_id::InstanceIDDriver*>(instance_id_driver_long);
  global_data.registration = device::cablev2::authenticator::Register(
      instance_id_driver, base::BindRepeating(OnContactEvent));

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

  DCHECK(!global_data.current_transaction);
  global_data.current_transaction =
      device::cablev2::authenticator::TransactWithPlaintextTransport(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          std::unique_ptr<device::cablev2::authenticator::Transport>(
              transport.release()),
          base::BindOnce(&TransactionComplete, env,
                         ScopedJavaGlobalRef<jobject>(cable_authenticator)));
}

static jboolean JNI_CableAuthenticator_StartQR(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    const JavaParamRef<jstring>& authenticator_name,
    const JavaParamRef<jstring>& qr_url) {
  GlobalData& global_data = GetGlobalData();
  base::Optional<DecodedQR> decoded_qr(
      DecodeQR(ConvertJavaStringToUTF8(qr_url)));
  if (!decoded_qr) {
    return false;
  }

  DCHECK(!global_data.current_transaction);
  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          global_data.network_context, global_data.root_secret,
          ConvertJavaStringToUTF8(authenticator_name), decoded_qr->secret,
          decoded_qr->peer_identity, global_data.registration->contact_id(),
          base::BindOnce(&TransactionComplete, env,
                         ScopedJavaGlobalRef<jobject>(cable_authenticator)));

  return true;
}

static void JNI_CableAuthenticator_StartFCM(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator) {
  GlobalData& global_data = GetGlobalData();
  const device::cablev2::authenticator::Registration::Event& event =
      *global_data.last_event.get();

  DCHECK(!global_data.current_transaction);
  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromFCM(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          global_data.network_context, global_data.root_secret,
          event.routing_id, event.tunnel_id, event.pairing_id,
          event.client_nonce,
          base::BindOnce(&TransactionComplete, env,
                         ScopedJavaGlobalRef<jobject>(cable_authenticator)));
}

static void JNI_CableAuthenticator_Stop(JNIEnv* env) {
  ResetGlobalData();
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
