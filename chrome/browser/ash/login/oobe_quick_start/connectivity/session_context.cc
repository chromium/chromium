// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"

#include <optional>

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

namespace ash::quick_start {

namespace {

// The keys used inside the kResumeQuickStartAfterRebootInfo local state
// pref dict.
constexpr char kPrepareForUpdateSessionIdKey[] = "session_id";
constexpr char kPrepareForUpdateAdvertisingIdKey[] = "advertising_id";
constexpr char kPrepareForUpdateSecondarySharedSecretKey[] =
    "secondary_shared_secret";
constexpr char kPrepareForUpdateDidTransferWifiKey[] = "did_transfer_wifi";

bool ShouldResumeAfterUpdate() {
  const base::Value::Dict& maybe_info =
      g_browser_process->local_state()->GetDict(
          prefs::kResumeQuickStartAfterRebootInfo);
  return maybe_info.FindString(kPrepareForUpdateSessionIdKey) &&
         maybe_info.FindString(kPrepareForUpdateAdvertisingIdKey);
}

}  // namespace

SessionContext::SessionContext() = default;

SessionContext::SessionContext(SessionId session_id,
                               AdvertisingId advertising_id,
                               SharedSecret shared_secret,
                               SharedSecret secondary_shared_secret,
                               bool is_resume_after_update)
    : session_id_(session_id),
      advertising_id_(advertising_id),
      shared_secret_(shared_secret),
      secondary_shared_secret_(secondary_shared_secret),
      is_resume_after_update_(is_resume_after_update) {}

SessionContext::SessionContext(const SessionContext& other) = default;

SessionContext& SessionContext::operator=(const SessionContext& other) =
    default;

SessionContext::~SessionContext() = default;

void SessionContext::FillOrResetSession() {
  is_resume_after_update_ = ShouldResumeAfterUpdate();
  QS_LOG(INFO)
      << "Going to fetch/generate session context. is_resume_after_update_: "
      << is_resume_after_update_;

  if (is_resume_after_update_) {
    FetchPersistedSessionContext();
  } else {
    PopulateRandomSessionContext();
  }
}

void SessionContext::CancelResume() {
  is_resume_after_update_ = false;
}

base::Value::Dict SessionContext::GetPrepareForUpdateInfo() {
  base::Value::Dict prepare_for_update_info;
  prepare_for_update_info.Set(kPrepareForUpdateSessionIdKey,
                              base::NumberToString(session_id_));
  prepare_for_update_info.Set(kPrepareForUpdateAdvertisingIdKey,
                              advertising_id_.ToString());
  std::string secondary_shared_secret_bytes(secondary_shared_secret_.begin(),
                                            secondary_shared_secret_.end());
  // The secondary_shared_secret_bytes string likely contains non-UTF-8
  // characters, which are disallowed in pref values. Base64Encode the string
  // for compatibility with prefs.
  prepare_for_update_info.Set(
      kPrepareForUpdateSecondarySharedSecretKey,
      base::Base64Encode(secondary_shared_secret_bytes));

  // We persist the bit representing completion of the Wi-Fi transfer, but Gaia
  // account setup happens after any updates are installed, so there is no need
  // to persist the Gaia account setup bit.
  prepare_for_update_info.Set(kPrepareForUpdateDidTransferWifiKey,
                              did_transfer_wifi_);

  return prepare_for_update_info;
}

void SessionContext::SetDidTransferWifi(bool did_transfer_wifi) {
  did_transfer_wifi_ = did_transfer_wifi;
}

void SessionContext::PopulateRandomSessionContext() {
  // The session_id_ should be in range (INT32_MAX, INT64_MAX].
  int64_t min = static_cast<int64_t>(INT32_MAX) + 1;
  int64_t range = INT64_MAX - INT32_MAX;
  session_id_ = min + base::RandGenerator(range);
  advertising_id_ = AdvertisingId();
  crypto::RandBytes(shared_secret_);
  crypto::RandBytes(secondary_shared_secret_);
  did_transfer_wifi_ = false;
}

void SessionContext::FetchPersistedSessionContext() {
  PrefService* prefs = g_browser_process->local_state();
  const base::Value::Dict& session_info =
      prefs->GetDict(prefs::kResumeQuickStartAfterRebootInfo);

  const std::string* session_id_str =
      session_info.FindString(kPrepareForUpdateSessionIdKey);
  CHECK(session_id_str)
      << "kPrepareForUpdateSessionIdKey missing in session info.";
  base::StringToUint64(*session_id_str, &session_id_);

  const std::string* advertising_id_str =
      session_info.FindString(kPrepareForUpdateAdvertisingIdKey);
  CHECK(advertising_id_str)
      << "kPrepareForUpdateAdvertisingIdKey missing in session info.";
  std::optional<AdvertisingId> maybe_advertising_id =
      AdvertisingId::ParseFromBase64(*advertising_id_str);
  if (!maybe_advertising_id.has_value()) {
    // TODO(b/234655072) Cancel Quick Start if this error occurs. The secondary
    // connection cannot bootstrap if the AdvertisingId doesn't match.
    prefs->ClearPref(prefs::kResumeQuickStartAfterRebootInfo);
    return;
  }
  advertising_id_ = maybe_advertising_id.value();

  const std::string* secondary_shared_secret_str =
      session_info.FindString(kPrepareForUpdateSecondarySharedSecretKey);
  CHECK(secondary_shared_secret_str)
      << "kPrepareForUpdateSecondarySharedSecretKey missing in session info.";
  DecodeSharedSecret(*secondary_shared_secret_str);

  std::optional<bool> did_transfer_wifi =
      session_info.FindBool(kPrepareForUpdateDidTransferWifiKey);
  did_transfer_wifi_ = did_transfer_wifi.value_or(true);

  prefs->ClearPref(prefs::kResumeQuickStartAfterRebootInfo);
}

void SessionContext::DecodeSharedSecret(
    const std::string& encoded_shared_secret) {
  std::string decoded_output;

  if (!base::Base64Decode(encoded_shared_secret, &decoded_output)) {
    // TODO(b/234655072) Cancel Quick Start if this error occurs. The secondary
    // connection can't bootstrap if the SharedSecret doesn't match the
    // secondary SharedSecret of the primary connection.
    QS_LOG(ERROR)
        << "Failed to decode the secondary shared secret from previous "
           "session. Encoded secondary shared secret: "
        << encoded_shared_secret;
    return;
  }

  if (decoded_output.length() != shared_secret_.size()) {
    // TODO(b/234655072) Cancel Quick Start if this error occurs.
    QS_LOG(ERROR) << "Decoded shared secret is an unexpected length. Decoded "
                     "shared secret output: "
                  << decoded_output;
    return;
  }

  for (size_t i = 0; i < decoded_output.length(); i++) {
    shared_secret_[i] = static_cast<uint8_t>(decoded_output[i]);
  }
}

}  // namespace ash::quick_start
