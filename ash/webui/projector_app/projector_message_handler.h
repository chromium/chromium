// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_

#include <set>

#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"
#include "ash/webui/projector_app/projector_xhr_sender.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

namespace ash {

// Enum to record the different errors that may occur in the Projector app.
enum class ProjectorError {
  kNone = 0,
  kOther,
  kTokenFetchFailure,
};

// Handles messages from the Projector WebUIs (i.e. chrome://projector).
class ProjectorMessageHandler : public content::WebUIMessageHandler,
                                public ProjectorAppClient::Observer {
 public:
  ProjectorMessageHandler();
  ProjectorMessageHandler(const ProjectorMessageHandler&) = delete;
  ProjectorMessageHandler& operator=(const ProjectorMessageHandler&) = delete;
  ~ProjectorMessageHandler() override;

  base::WeakPtr<ProjectorMessageHandler> GetWeakPtr();

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ProjectorAppClient:Observer:
  void OnNewScreencastPreconditionChanged(bool can_start) override;

  // Used to notify the SWA the SODA installation progress.
  void OnSodaProgress(int combined_progress);

  // Used to notify the SWA that SODA installation failed.
  void OnSodaError();

  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }

  // ProjectorAppClient::Observer:
  // Notifies the Projector SWA the pending screencasts' state change and
  // updates the pending list in Projector SWA.
  void OnScreencastsPendingStatusChanged(
      const std::set<PendingScreencast>& pending_screencast) override;

 private:
  // Requested by the Projector SWA to list the available accounts (primary and
  // secondary accounts) in the current session. The list of accounts will be
  // used in the account picker in the SWA.
  void GetAccounts(const base::Value::ConstListView args);

  // Requested by the Projector SWA to check if it is possible to start a new
  // Projector session.
  void CanStartProjectorSession(const base::Value::ConstListView args);

  // Requested by the Projector SWA to start a new Projector session if it is
  // possible.
  void StartProjectorSession(const base::Value::ConstListView args);

  // Requested by the Projector SWA to get access to the OAuth token for the
  // account email provided in the `args`.
  void GetOAuthTokenForAccount(const base::Value::ConstListView args);

  // Requested by the Projector SWA to send XHR request.
  void SendXhr(const base::Value::ConstListView args);

  // Requested by the Projector SWA on whether it should show the "new
  // screencast" button.
  void ShouldShowNewScreencastButton(const base::Value::ConstListView args);

  // Requested by the Projector SWA to check if SODA is not available and should
  // be downloaded. Returns false if the device doesn't support SODA.
  void ShouldDownloadSoda(const base::Value::ConstListView args);

  // Requested by the Projector SWA to trigger SODA installation.
  void InstallSoda(const base::Value::ConstListView args);

  // Called by the Projector SWA when an error occurred.
  void OnError(const base::Value::ConstListView args);

  // Called when OAuth token fetch request is completed by
  // ProjectorOAuthTokenFetcher. Resolves the javascript promise created by
  // ProjectorBrowserProxy.getOAuthTokenForAccount by calling the
  // `js_callback_id`.
  void OnAccessTokenRequestCompleted(const std::string& js_callback_id,
                                     const std::string& email,
                                     GoogleServiceAuthError error,
                                     const signin::AccessTokenInfo& info);

  // Called when the XHR request is completed. Resolves the javascript promise
  // created by ProjectorBrowserProxy.sendXhr by calling the `js_callback_id`.
  void OnXhrRequestCompleted(const std::string& js_callback_id,
                             bool success,
                             const std::string& response_body,
                             const std::string& error);

  // Requested by the Projector SWA to fetch a list of screencasts pending to
  // upload or failed to upload.
  void GetPendingScreencasts(const base::Value::ConstListView args);

  ProjectorOAuthTokenFetcher oauth_token_fetcher_;
  std::unique_ptr<ProjectorXhrSender> xhr_sender_;

  base::WeakPtrFactory<ProjectorMessageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_
