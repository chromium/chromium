// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"

#include "base/base64.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start {

namespace {

// The keys used inside the kResumeQuickStartAfterRebootInfo local state
// pref dict.
constexpr char kPrepareForUpdateRandomSessionIdKey[] = "random_session_id";
constexpr char kPrepareForUpdateSecondarySharedSecretKey[] =
    "secondary_shared_secret";

}  // namespace

SessionContext::SessionContext() {
  is_resume_after_update_ = g_browser_process->local_state()->GetBoolean(
      prefs::kShouldResumeQuickStartAfterReboot);
  QS_LOG(INFO)
      << "Going to fetch/generate session context. is_resume_after_update_: "
      << is_resume_after_update_;

  if (is_resume_after_update_) {
    FetchPersistedSessionContext();
  } else {
    random_session_id_ = RandomSessionId();
    crypto::RandBytes(shared_secret_);
    crypto::RandBytes(secondary_shared_secret_);
  }
}

SessionContext::SessionContext(RandomSessionId random_session_id,
                               SharedSecret shared_secret,
                               SharedSecret secondary_shared_secret,
                               bool is_resume_after_update)
    : random_session_id_(random_session_id),
      shared_secret_(shared_secret),
      secondary_shared_secret_(secondary_shared_secret),
      is_resume_after_update_(is_resume_after_update) {}

SessionContext::SessionContext(const SessionContext& other) = default;

SessionContext& SessionContext::operator=(const SessionContext& other) =
    default;

SessionContext::~SessionContext() = default;

base::Value::Dict SessionContext::GetPrepareForUpdateInfo() {
  base::Value::Dict prepare_for_update_info;
  prepare_for_update_info.Set(kPrepareForUpdateRandomSessionIdKey,
                              random_session_id_.ToString());
  std::string secondary_shared_secret_bytes(secondary_shared_secret_.begin(),
                                            secondary_shared_secret_.end());
  std::string secondary_shared_secret_base64;
  // The secondary_shared_secret_bytes string likely contains non-UTF-8
  // characters, which are disallowed in pref values. Base64Encode the string
  // for compatibility with prefs.
  base::Base64Encode(secondary_shared_secret_bytes,
                     &secondary_shared_secret_base64);
  prepare_for_update_info.Set(kPrepareForUpdateSecondarySharedSecretKey,
                              secondary_shared_secret_base64);

  return prepare_for_update_info;
}

void SessionContext::FetchPersistedSessionContext() {
  PrefService* prefs = g_browser_process->local_state();
  CHECK(prefs->GetBoolean(prefs::kShouldResumeQuickStartAfterReboot));
  prefs->ClearPref(prefs::kShouldResumeQuickStartAfterReboot);

  const base::Value::Dict& session_info =
      prefs->GetDict(prefs::kResumeQuickStartAfterRebootInfo);
  const std::string* random_session_id_str =
      session_info.FindString(kPrepareForUpdateRandomSessionIdKey);
  CHECK(random_session_id_str);
  absl::optional<RandomSessionId> maybe_random_session_id =
      RandomSessionId::ParseFromBase64(*random_session_id_str);
  if (!maybe_random_session_id.has_value()) {
    // TODO(b/234655072) Cancel Quick Start if this error occurs. The secondary
    // connection cannot bootstrap if the RandomSessionId doesn't match.
    prefs->ClearPref(prefs::kResumeQuickStartAfterRebootInfo);
    return;
  }
  random_session_id_ = maybe_random_session_id.value();

  const std::string* secondary_shared_secret_str =
      session_info.FindString(kPrepareForUpdateSecondarySharedSecretKey);
  CHECK(secondary_shared_secret_str);
  DecodeSharedSecret(*secondary_shared_secret_str);
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
