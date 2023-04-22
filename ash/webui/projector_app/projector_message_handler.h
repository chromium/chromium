// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_

#include <memory>

#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace signin {
struct AccessTokenInfo;
}  // namespace signin

class PrefService;

namespace ash {

class ProjectorXhrSender;
struct ProjectorScreencastVideo;

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
  explicit ProjectorMessageHandler(PrefService* pref_service);
  ProjectorMessageHandler(const ProjectorMessageHandler&) = delete;
  ProjectorMessageHandler& operator=(const ProjectorMessageHandler&) = delete;
  ~ProjectorMessageHandler() override;

  base::WeakPtr<ProjectorMessageHandler> GetWeakPtr();

  // content::WebUIMessageHandler:
  // TODO(b/237337607): chrome.send() is banned on ash. Migrate to Mojo instead.
  void RegisterMessages() override;

  // ProjectorAppClient::Observer:
  // Notifies the Projector SWA the pending screencasts' state change and
  // updates the pending list in Projector SWA.
  void OnScreencastsPendingStatusChanged(
      const PendingScreencastSet& pending_screencast) override;

  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }

 protected:
  // Called when the XHR request is completed. Resolves the javascript promise
  // created by ProjectorBrowserProxy.sendXhr by calling the `js_callback_id`.
  virtual void OnXhrRequestCompleted(const std::string& js_callback_id,
                                     bool success,
                                     const std::string& response_body,
                                     const std::string& error);

 private:
  // Requested by the Projector SWA to list the available accounts (primary and
  // secondary accounts) in the current session. The list of accounts will be
  // used in the account picker in the SWA.
  void GetAccounts(const base::Value::List& args);

  // Requested by the Projector SWA to start a new Projector session if it is
  // possible.
  void StartProjectorSession(const base::Value::List& args);

  // Requested by the Projector SWA to get access to the OAuth token for the
  // account email provided in the `args`.
  void GetOAuthTokenForAccount(const base::Value::List& args);

  // Requested by the Projector SWA to send XHR request.
  void SendXhr(const base::Value::List& args);

  // Called by the Projector SWA when an error occurred.
  void OnError(const base::Value::List& args);

  // Requested by the Projector SWA to get access to a particular user pref.
  void GetUserPref(const base::Value::List& args);

  // Requested by the Projector SWA to set the value of a user pref.
  void SetUserPref(const base::Value::List& args);

  // Requested by the Projector SWA to open the Chrome feedback dialog.
  void OpenFeedbackDialog(const base::Value::List& args);

  // Called when OAuth token fetch request is completed by
  // ProjectorOAuthTokenFetcher. Resolves the javascript promise created by
  // ProjectorBrowserProxy.getOAuthTokenForAccount by calling the
  // `js_callback_id`.
  void OnAccessTokenRequestCompleted(const std::string& js_callback_id,
                                     const std::string& email,
                                     GoogleServiceAuthError error,
                                     const signin::AccessTokenInfo& info);

  // Requested by the Projector SWA to fetch a list of screencasts pending to
  // upload or failed to upload.
  void GetPendingScreencasts(const base::Value::List& args);

  // Requested by the Projector SWA to fetch a single video from DriveFS with
  // the Drive item id specified by `args`.
  void GetVideo(const base::Value::List& args);

  // Called when video file fetch by item id request is complete. Resolves the
  // javascript promise created by ProjectorBrowserProxy.getScreencast by
  // calling the `js_callback_id`.
  void OnVideoLocated(const std::string& js_callback_id,
                      std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message);

  ProjectorOAuthTokenFetcher oauth_token_fetcher_;
  std::unique_ptr<ProjectorXhrSender> xhr_sender_;

  // Primary user pref service.
  const raw_ptr<PrefService, ExperimentalAsh> pref_service_;

  base::WeakPtrFactory<ProjectorMessageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_
