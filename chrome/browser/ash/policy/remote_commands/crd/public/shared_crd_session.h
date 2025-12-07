// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_SHARED_CRD_SESSION_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_SHARED_CRD_SESSION_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/policy/remote_commands/crd/public/crd_session_result_codes.h"

namespace policy {

// Shared Chrome Remote Desktop interface which starts the CRD host with
// Chrome enterprise parameters. The purpose of this interface is to provide
// shared access to the CRD host without exposing the remote command internals.
// The CRD host session is created and owned by the SharedCrdSession, and it
// will only be kept alive during the lifetime of the SharedCrdSession.
class SharedCrdSession {
 public:
  using AccessCodeCallback = base::OnceCallback<void(const std::string&)>;
  using ErrorCallback =
      base::OnceCallback<void(ExtendedStartCrdSessionResultCode,
                              const std::string&)>;
  using SessionFinishedCallback = base::OnceClosure;

  // The caller who initiated the request.
  // This should match `StartCrdSessionJobDelegate::RequestOrigin`.
  enum class RequestOrigin {
    kEnterpriseAdmin,
    kClassManagement,
  };

  // The audio playback mode for the CRD session.
  // This should match `StartCrdSessionJobDelegate::AudioPlayback`.
  enum class AudioPlayback {
    kLocalOnly,
    kRemoteAndLocal,
    kRemoteOnly,
  };

  // Session parameters used to start the CRD host.
  // This is a subset of the parameters inside of
  // `StartCrdSessionJobDelegate::SessionParameters`.
  struct SessionParameters {
    SessionParameters();
    ~SessionParameters();

    SessionParameters(const SessionParameters&);
    SessionParameters& operator=(const SessionParameters&);
    SessionParameters(SessionParameters&&);
    SessionParameters& operator=(SessionParameters&&);

    RequestOrigin request_origin;
    AudioPlayback audio_playback;
    std::optional<std::string> viewer_email;
    bool terminate_upon_input = false;
    bool show_confirmation_dialog = false;
    bool allow_file_transfer = false;
    bool allow_remote_input = true;
    bool allow_clipboard_sync = true;
  };

  virtual ~SharedCrdSession() = default;

  virtual void StartCrdHost(
      const SessionParameters& parameters,
      AccessCodeCallback success_callback,
      ErrorCallback error_callback,
      SessionFinishedCallback session_finished_callback) = 0;

  virtual void TerminateSession() = 0;
};

}  // namespace policy
#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_PUBLIC_SHARED_CRD_SESSION_H_
