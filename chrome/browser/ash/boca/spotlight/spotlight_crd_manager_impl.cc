// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/spotlight/spotlight_crd_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session_provider.h"

namespace ash::boca {
namespace {
using SessionParameters = policy::SharedCrdSession::SessionParameters;

// TODO: dorianbrandon - Log result to UMA.
void LogCrdError(policy::ExtendedStartCrdSessionResultCode result_code,
                 const std::string& message) {
  LOG(WARNING) << "[Boca] Failed to start Spotlight session on student due to "
               << "CRD error (code " << static_cast<int>(result_code)
               << ", message '" << message << "')";
}
}  // namespace

SpotlightCrdManagerImpl::SpotlightCrdManagerImpl(PrefService* pref_service)
    : provider_(
          std::make_unique<policy::SharedCrdSessionProvider>(pref_service)),
      crd_session_(provider_->GetCrdSession()) {}

SpotlightCrdManagerImpl::SpotlightCrdManagerImpl(
    std::unique_ptr<policy::SharedCrdSession> crd_session)
    : crd_session_(std::move(crd_session)) {
  CHECK_IS_TEST();
}

SpotlightCrdManagerImpl::~SpotlightCrdManagerImpl() = default;

void SpotlightCrdManagerImpl::OnSessionStarted(
    const std::string& teacher_email) {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }
  teacher_email_ = teacher_email;
}

void SpotlightCrdManagerImpl::OnSessionEnded() {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }
  teacher_email_ = "";
  crd_session_->TerminateSession();
}

void SpotlightCrdManagerImpl::InitiateSpotlightSession(
    ConnectionCodeCallback callback) {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }
  if (teacher_email_.empty()) {
    LOG(WARNING)
        << "[Boca] Tried to initiate Spotlight without valid teacher email.";
    return;
  }

  SessionParameters parameters;
  parameters.viewer_email = teacher_email_;
  parameters.allow_file_transfer = false;
  parameters.show_confirmation_dialog = false;
  parameters.terminate_upon_input = false;

  crd_session_->StartCrdHost(parameters, std::move(callback),
                             base::BindOnce(&LogCrdError));
}
}  // namespace ash::boca
