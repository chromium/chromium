// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/shared_crd_session_impl.h"

#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_uma_logger.h"
#include "chrome/browser/ash/policy/remote_commands/crd/start_crd_session_job_delegate.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "device_management_backend.pb.h"

namespace policy {

namespace {

DeviceOAuth2TokenService* GetOAuthService() {
  return DeviceOAuth2TokenServiceFactory::Get();
}

std::string GetRobotAccountUserName(const DeviceOAuth2TokenService* service) {
  CoreAccountId account_id = CHECK_DEREF(service).GetRobotAccountId();
  return account_id.ToString();
}

// Logs the session length and type to UMA. Also allows consumers of
// `policy::SharedCrdSession` to provide their own callback for session end.
void OnCrdSessionFinished(base::OnceClosure session_finished_callback,
                          base::TimeDelta session_duration) {
  CrdUmaLogger(CrdSessionType::REMOTE_SUPPORT_SESSION,
               UserSessionType::AFFILIATED_USER_SESSION)
      .LogSessionDuration(session_duration);

  std::move(session_finished_callback).Run();
}
}  // namespace

SharedCrdSessionImpl::SharedCrdSessionImpl(Delegate& delegate)
    : delegate_(delegate),
      robot_account_id_(GetRobotAccountUserName(GetOAuthService())) {}

SharedCrdSessionImpl::SharedCrdSessionImpl(Delegate& delegate,
                                           std::string_view robot_account_id)
    : delegate_(delegate), robot_account_id_(robot_account_id) {
  CHECK_IS_TEST();
}

SharedCrdSessionImpl::~SharedCrdSessionImpl() = default;

void SharedCrdSessionImpl::StartCrdHost(
    const SessionParameters& parameters,
    AccessCodeCallback success_callback,
    ErrorCallback error_callback,
    SessionFinishedCallback session_finished_callback) {
  if (delegate_->HasActiveSession()) {
    CRD_VLOG(1) << "Terminating active session";
    delegate_->TerminateSession();
    CHECK(!delegate_->HasActiveSession());
  }

  StartCrdSessionJobDelegate::SessionParameters session_parameters;
  session_parameters.user_name = robot_account_id_;
  session_parameters.admin_email = parameters.viewer_email;
  session_parameters.allow_file_transfer = parameters.allow_file_transfer;
  session_parameters.show_confirmation_dialog =
      parameters.show_confirmation_dialog;
  session_parameters.terminate_upon_input = parameters.terminate_upon_input;
  session_parameters.allow_remote_input = parameters.allow_remote_input;
  session_parameters.allow_clipboard_sync = parameters.allow_clipboard_sync;
  session_parameters.request_origin =
      ConvertToStartCrdSessionJobDelegateRequestOrigin(
          parameters.request_origin);

  CRD_VLOG(1) << "Starting CRD host and retrieving CRD access code";
  delegate_->StartCrdHostAndGetCode(
      session_parameters, std::move(success_callback),
      std::move(error_callback),
      base::BindOnce(&OnCrdSessionFinished,
                     std::move(session_finished_callback)));
}

void SharedCrdSessionImpl::TerminateSession() {
  delegate_->TerminateSession();
}

}  // namespace policy
