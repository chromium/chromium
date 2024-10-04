// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

// These "headers" actually contain several function definitions and thus can
// only be included once across Chromium.
#include "base/time/time.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/features/cablev2_authenticator/jni_headers/BLEAdvert_jni.h"
#include "chrome/android/features/cablev2_authenticator/jni_headers/CableAuthenticator_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace {

// CableV2MobileEvent enumerates several steps that occur during a caBLEv2
// transaction. Do not change the assigned value since they are used in
// histograms, only append new values. Keep synced with enums.xml.
enum class CableV2MobileEvent {
  kQRRead = 0,
  kServerLink = 1,
  kCloudMessage = 2,
  kDeprecatedUsb = 3,
  kSetup = 4,
  kTunnelServerConnected = 5,
  kHandshakeCompleted = 6,
  kRequestReceived = 7,
  kCTAPError = 8,
  kUnlink = 9,
  kNeedInteractive = 10,
  kInteractionReady = 11,
  kLinkingNotRequested = 12,
  kDeprecatedUsbSuccess = 13,
  kStoppedWhileAwaitingTunnelServerConnection = 14,
  kStoppedWhileAwaitingHandshake = 15,
  kStoppedWhileAwaitingRequest = 16,
  kStoppedWhileAuthenticating = 17,
  kStrayGetAssertionResponse = 18,
  kGetAssertionStarted = 19,
  kGetAssertionComplete = 20,
  kFirstTransactionDone = 21,
  kContactIDNotReady = 22,
  kBluetoothAdvertisePermissionRequested = 23,
  kBluetoothAdvertisePermissionGranted = 24,
  kBluetoothAdvertisePermissionRejected = 25,

  kMaxValue = 25,
};

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
  kEOFWhileProcessing = 11,
  kDiscoverableCredentialsRejected = 12,

  kMaxValue = 12,
};

// JavaByteArrayToByteVector returns a copy of the contents of |data|.
std::vector<uint8_t> JavaByteArrayToByteVector(
    JNIEnv* env,
    const JavaParamRef<jbyteArray>& data) {
  if (data.is_null()) {
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> ret;
  base::android::JavaByteArrayToByteVector(env, data, &ret);
  return ret;
}

// GlobalData holds all the state for ongoing security key operations. Since
// there are ultimately only one human user, concurrent requests are not
// supported.
struct GlobalData {
  raw_ptr<JNIEnv> env = nullptr;
  // instance_num is incremented for each new |Transaction| created and returned
  // to Java to serve as a "handle". This prevents commands intended for a
  // previous transaction getting applied to a replacement. The zero value is
  // reserved so that functions can still return that to indicate an error.
  jlong instance_num = 1;

  std::optional<std::array<uint8_t, device::cablev2::kRootSecretSize>>
      root_secret;
  raw_ptr<network::mojom::NetworkContext> network_context = nullptr;

  // event_to_record_if_stopped contains an event to record with UMA if the
  // activity is stopped. This is updated as a transaction progresses.
  std::optional<CableV2MobileEvent> event_to_record_if_stopped;

  // registration is a non-owning pointer to the global |Registration|.
  raw_ptr<device::cablev2::authenticator::Registration> registration = nullptr;

  // current_transaction holds the |Transaction| that is currently active.
  std::unique_ptr<device::cablev2::authenticator::Transaction>
      current_transaction;

  // pending_make_credential_callback holds the callback that the
  // |Authenticator| expects to be run once a makeCredential operation has
  // completed.
  std::optional<
      device::cablev2::authenticator::Platform::MakeCredentialCallback>
      pending_make_credential_callback;
  // pending_get_assertion_callback holds the callback that the
  // |Authenticator| expects to be run once a getAssertion operation has
  // completed.
  std::optional<device::cablev2::authenticator::Platform::GetAssertionCallback>
      pending_get_assertion_callback;

  // server_link_tunnel_id contains the derived tunnel ID for server–link
  // transactions. May be |nullopt| if the current transaction is not
  // server-linked. This is used as an event ID when logging.
  std::optional<std::array<uint8_t, device::cablev2::kTunnelIdSize>>
      server_link_tunnel_id;
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
}

void RecordEvent(const GlobalData* global_data, CableV2MobileEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.CableV2.MobileEvent", event);
}

void RecordResult(const GlobalData* global_data, CableV2MobileResult result) {
  base::UmaHistogramEnumeration("WebAuthentication.CableV2.MobileResult",
                                result);
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
  const raw_ptr<JNIEnv> env_;
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

  ~AndroidPlatform() override = default;

  // Platform:
  void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr params,
      MakeCredentialCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_make_credential_callback);
    global_data.pending_make_credential_callback = std::move(callback);

    std::vector<uint8_t> params_bytes =
        blink::mojom::PublicKeyCredentialCreationOptions::Serialize(&params);

    Java_CableAuthenticator_makeCredential(env_, cable_authenticator_,
                                           params_bytes);
  }

  void GetAssertion(blink::mojom::PublicKeyCredentialRequestOptionsPtr params,
                    GetAssertionCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    GlobalData& global_data = GetGlobalData();
    DCHECK(!global_data.pending_get_assertion_callback);
    global_data.pending_get_assertion_callback = std::move(callback);

    std::vector<uint8_t> params_bytes =
        blink::mojom::PublicKeyCredentialRequestOptions::Serialize(&params);

    RecordEvent(&global_data, CableV2MobileEvent::kGetAssertionStarted);
    Java_CableAuthenticator_getAssertion(env_, cable_authenticator_,
                                         params_bytes);
  }

  void OnStatus(Status status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    LOG(ERROR) << __func__ << " " << static_cast<int>(status);

    GlobalData& global_data = GetGlobalData();
    CableV2MobileEvent event;
    switch (status) {
      case Status::TUNNEL_SERVER_CONNECT:
        event = CableV2MobileEvent::kTunnelServerConnected;
        tunnel_server_connect_time_.emplace();
        global_data.event_to_record_if_stopped =
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
        global_data.event_to_record_if_stopped =
            CableV2MobileEvent::kStoppedWhileAwaitingRequest;
        break;
      case Status::REQUEST_RECEIVED:
        event = CableV2MobileEvent::kRequestReceived;
        global_data.event_to_record_if_stopped =
            CableV2MobileEvent::kStoppedWhileAuthenticating;
        break;
      case Status::CTAP_ERROR:
        event = CableV2MobileEvent::kCTAPError;
        break;
      case Status::FIRST_TRANSACTION_DONE:
        global_data.event_to_record_if_stopped.reset();
        event = CableV2MobileEvent::kFirstTransactionDone;
        break;
    }
    RecordEvent(&global_data, event);

    if (!cable_authenticator_) {
      return;
    }

    Java_CableAuthenticator_onStatus(env_, cable_authenticator_,
                                     static_cast<int>(status));
  }

  void OnCompleted(std::optional<Error> maybe_error) override {
    LOG(ERROR) << __func__ << " "
               << (maybe_error ? static_cast<int>(*maybe_error) : -1);
    GlobalData& global_data = GetGlobalData();
    global_data.event_to_record_if_stopped.reset();

    CableV2MobileResult result = CableV2MobileResult::kSuccess;
    if (maybe_error) {
      switch (*maybe_error) {
        case Error::UNEXPECTED_EOF:
          result = CableV2MobileResult::kUnexpectedEOF;
          break;
        case Error::EOF_WHILE_PROCESSING:
          result = CableV2MobileResult::kEOFWhileProcessing;
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
        case Error::AUTHENTICATOR_SELECTION_RECEIVED:
        case Error::DISCOVERABLE_CREDENTIALS_REQUEST:
          result = CableV2MobileResult::kDiscoverableCredentialsRejected;
          break;
        case Error::INTERNAL_ERROR:
        case Error::SERVER_LINK_WRONG_LENGTH:
        case Error::SERVER_LINK_NOT_ON_CURVE:
        case Error::NO_SCREENLOCK:
        case Error::NO_BLUETOOTH_PERMISSION:
        case Error::QR_URI_ERROR:
        case Error::INVALID_JSON:
          result = CableV2MobileResult::kInternalError;
          break;
      }
    }
    RecordResult(&global_data, result);

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
  const raw_ptr<JNIEnv> env_;
  ScopedJavaGlobalRef<jobject> cable_authenticator_;
  std::optional<base::ElapsedTimer> tunnel_server_connect_time_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AndroidPlatform> weak_factory_{this};
};

}  // anonymous namespace

// These functions are the entry points for CableAuthenticator.java and
// BLEHandler.java calling into C++.

static void JNI_CableAuthenticator_Setup(JNIEnv* env,
                                         jlong registration_long,
                                         jlong network_context_long,
                                         std::vector<uint8_t>& root_secret) {
  GlobalData& global_data = GetGlobalData();

  // The root_secret may not be provided when triggered for server-link. It
  // won't be used in that case either, but we need to be able to grab it if
  // setup() is called called for a different type of exchange.
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

  RecordEvent(&global_data, CableV2MobileEvent::kSetup);
  global_data.env = env;

  static_assert(sizeof(jlong) >= sizeof(void*), "");
  global_data.registration =
      reinterpret_cast<device::cablev2::authenticator::Registration*>(
          registration_long);
  global_data.registration->PrepareContactID();

  global_data.network_context =
      reinterpret_cast<network::mojom::NetworkContext*>(network_context_long);
}

static jlong JNI_CableAuthenticator_StartQR(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    std::string& authenticator_name,
    std::string& qr_string,
    jboolean link) {
  GlobalData& global_data = GetGlobalData();
  RecordEvent(&global_data, CableV2MobileEvent::kQRRead);

  std::optional<device::cablev2::qr::Components> decoded_qr(
      device::cablev2::qr::Parse(qr_string));
  if (!decoded_qr) {
    FIDO_LOG(ERROR) << "Failed to decode QR: " << qr_string;
    RecordResult(&global_data, CableV2MobileResult::kInvalidQR);
    return 0;
  }

  if (!link) {
    RecordEvent(&global_data, CableV2MobileEvent::kLinkingNotRequested);
  } else if (!global_data.registration->contact_id()) {
    LOG(ERROR) << "Contact ID was not ready for QR transaction";
    RecordEvent(&global_data, CableV2MobileEvent::kContactIDNotReady);
  }

  global_data.event_to_record_if_stopped =
      CableV2MobileEvent::kStoppedWhileAwaitingTunnelServerConnection;
  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromQRCodeDeprecated(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          global_data.network_context, *global_data.root_secret,
          authenticator_name, decoded_qr->secret, decoded_qr->peer_identity,
          link ? global_data.registration->contact_id() : std::nullopt);

  return ++global_data.instance_num;
}

std::tuple<std::array<uint8_t, device::kP256X962Length>,
           std::array<uint8_t, device::cablev2::kQRSecretSize>,
           std::array<uint8_t, device::cablev2::kTunnelIdSize>>
ParseServerLinkData(std::vector<uint8_t>& server_link_data) {
  // validateServerLinkData should have been called to check this already.
  CHECK_EQ(server_link_data.size(),
           device::kP256X962Length + device::cablev2::kQRSecretSize);

  std::array<uint8_t, device::kP256X962Length> peer_identity;
  memcpy(peer_identity.data(), server_link_data.data(),
         device::kP256X962Length);

  std::array<uint8_t, device::cablev2::kQRSecretSize> qr_secret;
  memcpy(qr_secret.data(), server_link_data.data() + device::kP256X962Length,
         device::cablev2::kQRSecretSize);

  const std::array<uint8_t, device::cablev2::kTunnelIdSize> tunnel_id =
      device::cablev2::Derive<device::cablev2::kTunnelIdSize>(
          qr_secret, base::span<uint8_t>(),
          device::cablev2::DerivedValueType::kTunnelID);

  return std::make_tuple(peer_identity, qr_secret, tunnel_id);
}

static jlong JNI_CableAuthenticator_StartServerLink(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    std::vector<uint8_t>& server_link_data) {
  GlobalData& global_data = GetGlobalData();

  auto server_link_values = ParseServerLinkData(server_link_data);
  auto peer_identity = std::get<0>(server_link_values);
  auto qr_secret = std::get<1>(server_link_values);
  global_data.server_link_tunnel_id = std::get<2>(server_link_values);

  // Sending pairing information is disabled when doing a server-linked
  // connection, thus the root secret and authenticator name will not be used.
  std::array<uint8_t, device::cablev2::kRootSecretSize> dummy_root_secret = {0};
  std::string dummy_authenticator_name = "";
  global_data.event_to_record_if_stopped =
      CableV2MobileEvent::kStoppedWhileAwaitingTunnelServerConnection;
  RecordEvent(&global_data, CableV2MobileEvent::kServerLink);

  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromQRCodeDeprecated(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          global_data.network_context, dummy_root_secret,
          dummy_authenticator_name, qr_secret, peer_identity, std::nullopt);

  return ++global_data.instance_num;
}

static jlong JNI_CableAuthenticator_StartCloudMessage(
    JNIEnv* env,
    const JavaParamRef<jobject>& cable_authenticator,
    std::vector<uint8_t>& serialized_event) {
  GlobalData& global_data = GetGlobalData();
  RecordEvent(&global_data, CableV2MobileEvent::kCloudMessage);

  auto event =
      device::cablev2::authenticator::Registration::Event::FromSerialized(
          serialized_event);
  if (!event) {
    LOG(ERROR) << "Failed to parse event";
    return 0;
  }

  DCHECK((event->source ==
          device::cablev2::authenticator::Registration::Type::LINKING) ==
         event->contact_id.has_value());

  // There is deliberately no check for |!global_data.current_transaction|
  // because multiple Cloud messages may come in from different paired devices.
  // Only the most recent is processed.
  global_data.event_to_record_if_stopped =
      CableV2MobileEvent::kStoppedWhileAwaitingTunnelServerConnection;
  global_data.current_transaction =
      device::cablev2::authenticator::TransactFromFCMDeprecated(
          std::make_unique<AndroidPlatform>(env, cable_authenticator),
          global_data.network_context, *global_data.root_secret,
          event->routing_id, event->tunnel_id, event->pairing_id,
          event->client_nonce, event->contact_id);

  return ++global_data.instance_num;
}

static void JNI_CableAuthenticator_Stop(JNIEnv* env, jlong instance_num) {
  GlobalData& global_data = GetGlobalData();
  if (global_data.instance_num == instance_num) {
    ResetGlobalData();
  }
}

static int JNI_CableAuthenticator_ValidateServerLinkData(
    JNIEnv* env,
    std::vector<uint8_t>& data) {
  if (data.size() != device::kP256X962Length + device::cablev2::kQRSecretSize) {
    RecordResult(nullptr, CableV2MobileResult::kInvalidServerLink);
    return static_cast<int>(device::cablev2::authenticator::Platform::Error::
                                SERVER_LINK_WRONG_LENGTH);
  }

  base::span<const uint8_t> x962 =
      base::make_span(data).first(device::kP256X962Length);
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), x962.data(), x962.size(),
                          /*ctx=*/nullptr)) {
    RecordResult(nullptr, CableV2MobileResult::kInvalidServerLink);
    return static_cast<int>(device::cablev2::authenticator::Platform::Error::
                                SERVER_LINK_NOT_ON_CURVE);
  }

  return 0;
}

static int JNI_CableAuthenticator_ValidateQRURI(JNIEnv* env,
                                                std::string& qr_string) {
  if (!device::cablev2::qr::Parse(qr_string)) {
    RecordResult(nullptr, CableV2MobileResult::kInvalidQR);
    return static_cast<int>(
        device::cablev2::authenticator::Platform::Error::QR_URI_ERROR);
  }

  return 0;
}

static void JNI_CableAuthenticator_OnActivityStop(JNIEnv* env,
                                                  jlong instance_num) {
  GlobalData& global_data = GetGlobalData();
  if (global_data.event_to_record_if_stopped &&
      global_data.instance_num == instance_num) {
    RecordEvent(&global_data, *global_data.event_to_record_if_stopped);
    global_data.event_to_record_if_stopped.reset();
  }
}

static void JNI_CableAuthenticator_OnAuthenticatorAttestationResponse(
    JNIEnv* env,
    jint ctap_status,
    std::vector<uint8_t>& attestation_object,
    jboolean prf_enabled) {
  GlobalData& global_data = GetGlobalData();

  if (!global_data.pending_make_credential_callback) {
    return;
  }
  auto callback = std::move(*global_data.pending_make_credential_callback);
  global_data.pending_make_credential_callback.reset();

  std::move(callback).Run(ctap_status, attestation_object, prf_enabled);
}

static void JNI_CableAuthenticator_OnAuthenticatorAssertionResponse(
    JNIEnv* env,
    jint ctap_status,
    const JavaParamRef<jbyteArray>& jresponse_bytes) {
  GlobalData& global_data = GetGlobalData();
  RecordEvent(&global_data, CableV2MobileEvent::kGetAssertionComplete);

  if (!global_data.pending_get_assertion_callback) {
    RecordEvent(&global_data, CableV2MobileEvent::kStrayGetAssertionResponse);
    return;
  }
  auto callback = std::move(*global_data.pending_get_assertion_callback);
  global_data.pending_get_assertion_callback.reset();

  if (ctap_status ==
      static_cast<jint>(device::CtapDeviceResponseCode::kSuccess)) {
    std::vector<uint8_t> response_bytes =
        JavaByteArrayToByteVector(env, jresponse_bytes);
    auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
    if (blink::mojom::GetAssertionAuthenticatorResponse::Deserialize(
            response_bytes.data(), response_bytes.size(), &response)) {
      std::move(callback).Run(ctap_status, std::move(response));
      return;
    }

    ctap_status =
        static_cast<jint>(device::CtapDeviceResponseCode::kCtap2ErrOther);
  }

  std::move(callback).Run(ctap_status, nullptr);
}
