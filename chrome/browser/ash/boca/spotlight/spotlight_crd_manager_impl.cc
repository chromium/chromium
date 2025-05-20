// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/spotlight/spotlight_crd_manager_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/boca/spotlight/spotlight_notification_bubble_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session_provider.h"

namespace ash::boca {
namespace {
using SessionParameters = policy::SharedCrdSession::SessionParameters;
constexpr char kCrdResultUma[] = "Enterprise.Boca.Spotlight.Crd.Result";

void LogCrdError(policy::ExtendedStartCrdSessionResultCode result_code,
                 const std::string& message) {
  base::UmaHistogramEnumeration(kCrdResultUma, result_code);
  LOG(WARNING) << "[Boca] Failed to start Spotlight session on student due to "
               << "CRD error (code " << static_cast<int>(result_code)
               << ", message '" << message << "')";
}
}  // namespace

SpotlightCrdManagerImpl::SpotlightCrdManagerImpl(PrefService* pref_service)
    : provider_(
          std::make_unique<policy::SharedCrdSessionProvider>(pref_service)),
      crd_session_(provider_->GetCrdSession()),
      persistent_bubble_controller_(
          std::make_unique<SpotlightNotificationBubbleController>()) {}

SpotlightCrdManagerImpl::SpotlightCrdManagerImpl(
    std::unique_ptr<policy::SharedCrdSession> crd_session,
    std::unique_ptr<SpotlightNotificationBubbleController>
        persistent_bubble_controller)
    : crd_session_(std::move(crd_session)),
      persistent_bubble_controller_(std::move(persistent_bubble_controller)) {
  CHECK_IS_TEST();
}

SpotlightCrdManagerImpl::~SpotlightCrdManagerImpl() = default;

void SpotlightCrdManagerImpl::OnSessionEnded() {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }
  crd_session_->TerminateSession();
  persistent_bubble_controller_->OnSessionEnded();
}

void SpotlightCrdManagerImpl::InitiateSpotlightSession(
    ConnectionCodeCallback callback,
    const std::string& requester_email) {
  if (!ash::features::IsBocaSpotlightEnabled()) {
    return;
  }

  SessionParameters parameters;
  parameters.viewer_email = requester_email;
  parameters.allow_file_transfer = false;
  parameters.show_confirmation_dialog = false;
  parameters.terminate_upon_input = false;
  parameters.allow_remote_input = false;
  parameters.allow_clipboard_sync = false;
  parameters.request_origin =
      policy::SharedCrdSession::RequestOrigin::kClassManagement;

  crd_session_->StartCrdHost(
      parameters, std::move(callback), base::BindOnce(&LogCrdError),
      base::BindOnce(&SpotlightCrdManagerImpl::HidePersistentNotification,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SpotlightCrdManagerImpl::ShowPersistentNotification(
    const std::string& teacher_name) {
  persistent_bubble_controller_->ShowNotificationBubble(teacher_name);
}

void SpotlightCrdManagerImpl::HidePersistentNotification() {
  persistent_bubble_controller_->HideNotificationBubble();
}
}  // namespace ash::boca
