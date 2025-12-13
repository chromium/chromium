// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/shared_crd_session_wrapper_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/shared_crd_session_provider.h"

namespace ash::boca {
namespace {

void OnError(base::OnceClosure error_callback,
             policy::ExtendedStartCrdSessionResultCode,
             const std::string&) {
  std::move(error_callback).Run();
}

}  // namespace

SharedCrdSessionWrapperImpl::SharedCrdSessionWrapperImpl(
    std::unique_ptr<policy::SharedCrdSessionProvider> crd_session_provider)
    : crd_session_provider_(std::move(crd_session_provider)),
      crd_session_(crd_session_provider_->GetCrdSession()) {}

SharedCrdSessionWrapperImpl::~SharedCrdSessionWrapperImpl() = default;

void SharedCrdSessionWrapperImpl::StartCrdHost(
    const std::string& receiver_email,
    base::OnceCallback<void(const std::string&)> success_callback,
    base::OnceClosure error_callback,
    base::OnceClosure session_finished_callback) {
  policy::SharedCrdSession::SessionParameters parameters;
  parameters.viewer_email = receiver_email;
  parameters.allow_file_transfer = false;
  parameters.show_confirmation_dialog = false;
  parameters.terminate_upon_input = false;
  parameters.allow_remote_input = false;
  parameters.allow_clipboard_sync = false;
  parameters.request_origin =
      policy::SharedCrdSession::RequestOrigin::kClassManagement;
  // When remoting to a kiosk receiver, audio should play on the kiosk only.
  parameters.audio_playback =
      policy::SharedCrdSession::AudioPlayback::kRemoteOnly;
  crd_session_->StartCrdHost(
      parameters, std::move(success_callback),
      base::BindOnce(&OnError, std::move(error_callback)),
      std::move(session_finished_callback));
}

void SharedCrdSessionWrapperImpl::TerminateSession() {
  crd_session_->TerminateSession();
}

}  // namespace ash::boca
