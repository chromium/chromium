// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_message_handler.h"

#include <memory>
#include <string>

#include "ash/public/cpp/projector/projector_controller.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Response keys.
constexpr char kUserName[] = "name";
constexpr char kUserEmail[] = "email";
constexpr char kUserPictureURL[] = "pictureURL";
constexpr char kIsPrimaryUser[] = "isPrimaryUser";
constexpr char kToken[] = "token";
constexpr char kExpirationTime[] = "expirationTime";
constexpr char kError[] = "error";
constexpr char kOAuthTokenInfo[] = "oauthTokenInfo";
constexpr char kXhrSuccess[] = "success";
constexpr char kXhrResponseBody[] = "response";
constexpr char kXhrError[] = "error";

// Projector Error Strings.
constexpr char kNoneStr[] = "NONE";
constexpr char kOtherStr[] = "OTHER";
constexpr char kTokenFetchFailureStr[] = "TOKEN_FETCH_FAILURE";

base::Value AccessTokenInfoToValue(const signin::AccessTokenInfo& info) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kToken, base::Value(info.token));
  value.SetKey(kExpirationTime, base::TimeToValue(info.expiration_time));
  return value;
}

std::string ProjectorErrorToString(ProjectorError mode) {
  switch (mode) {
    case ProjectorError::kNone:
      return kNoneStr;
    case ProjectorError::kTokenFetchFailure:
      return kTokenFetchFailureStr;
    case ProjectorError::kOther:
      return kOtherStr;
  }
}

base::Value ScreencastListToValue(
    const std::set<PendingScreencast>& screencasts) {
  std::vector<base::Value> value;
  value.reserve(screencasts.size());
  for (const auto& item : screencasts)
    value.push_back(item.ToValue());

  return base::Value(std::move(value));
}

}  // namespace

ProjectorMessageHandler::ProjectorMessageHandler()
    : content::WebUIMessageHandler(),
      xhr_sender_(std::make_unique<ProjectorXhrSender>(
          ProjectorAppClient::Get()->GetUrlLoaderFactory())) {
  ProjectorAppClient::Get()->AddObserver(this);
}

ProjectorMessageHandler::~ProjectorMessageHandler() {
  ProjectorAppClient::Get()->RemoveObserver(this);
}

base::WeakPtr<ProjectorMessageHandler> ProjectorMessageHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProjectorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAccounts", base::BindRepeating(&ProjectorMessageHandler::GetAccounts,
                                         base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "canStartProjectorSession",
      base::BindRepeating(&ProjectorMessageHandler::CanStartProjectorSession,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "startProjectorSession",
      base::BindRepeating(&ProjectorMessageHandler::StartProjectorSession,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getOAuthTokenForAccount",
      base::BindRepeating(&ProjectorMessageHandler::GetOAuthTokenForAccount,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "onError", base::BindRepeating(&ProjectorMessageHandler::OnError,
                                     base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "sendXhr", base::BindRepeating(&ProjectorMessageHandler::SendXhr,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "shouldShowNewScreencastButton",
      base::BindRepeating(
          &ProjectorMessageHandler::ShouldShowNewScreencastButton,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "shouldDownloadSoda",
      base::BindRepeating(&ProjectorMessageHandler::ShouldDownloadSoda,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "installSoda", base::BindRepeating(&ProjectorMessageHandler::InstallSoda,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPendingScreencasts",
      base::BindRepeating(&ProjectorMessageHandler::GetPendingScreencasts,
                          base::Unretained(this)));
}

void ProjectorMessageHandler::OnSodaProgress(int combined_progress) {
  AllowJavascript();
  FireWebUIListener("onSodaInstallProgressUpdated",
                    base::Value(combined_progress));
}

void ProjectorMessageHandler::OnSodaError() {
  AllowJavascript();
  FireWebUIListener("onSodaInstallError");
}

void ProjectorMessageHandler::OnScreencastsPendingStatusChanged(
    const std::set<PendingScreencast>& pending_screencast) {
  AllowJavascript();
  FireWebUIListener("onScreencastsStateChange",
                    ScreencastListToValue(pending_screencast));
}

void ProjectorMessageHandler::OnNewScreencastPreconditionChanged(
    bool can_start) {
  AllowJavascript();
  FireWebUIListener("onNewScreencastPreconditionChanged",
                    base::Value(can_start));
}

void ProjectorMessageHandler::GetAccounts(base::Value::ConstListView args) {
  AllowJavascript();

  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);
  auto* controller = ProjectorController::Get();
  DCHECK(controller);

  const std::vector<AccountInfo> accounts = oauth_token_fetcher_.GetAccounts();
  const CoreAccountInfo primary_account =
      oauth_token_fetcher_.GetPrimaryAccountInfo();

  std::vector<base::Value> response;
  response.reserve(accounts.size());
  for (const auto& info : accounts) {
    base::Value account_info(base::Value::Type::DICTIONARY);
    account_info.SetKey(kUserName, base::Value(info.full_name));
    account_info.SetKey(kUserEmail, base::Value(info.email));
    account_info.SetKey(kUserPictureURL, base::Value(info.picture_url));
    account_info.SetKey(kIsPrimaryUser,
                        base::Value(info.gaia == primary_account.gaia));
    response.push_back(std::move(account_info));
  }

  ResolveJavascriptCallback(args[0], base::Value(std::move(response)));
}

void ProjectorMessageHandler::CanStartProjectorSession(
    base::Value::ConstListView args) {
  AllowJavascript();

  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);

  ResolveJavascriptCallback(
      args[0], base::Value(ProjectorController::Get()->CanStartNewSession()));
}

void ProjectorMessageHandler::StartProjectorSession(
    base::Value::ConstListView args) {
  AllowJavascript();

  // There are two arguments. The first is the callback and the second is a list
  // containing the account which we need to start the recording with.
  DCHECK_EQ(args.size(), 2u);

  const auto& func_args = args[1];
  DCHECK(func_args.is_list());

  // The first entry is the drive directory to save the screen cast to.
  // TODO(b/177959166): Pass the directory to ProjectorController when starting
  // a new session.
  DCHECK_EQ(func_args.GetList().size(), 1u);

  // TODO(b/195113693): Start the projector session with the selected account
  // and folder.
  auto* controller = ProjectorController::Get();
  if (!controller->CanStartNewSession()) {
    ResolveJavascriptCallback(args[0], base::Value(false));
    return;
  }

  controller->StartProjectorSession(func_args.GetList()[0].GetString());
  ResolveJavascriptCallback(args[0], base::Value(true));
}

void ProjectorMessageHandler::GetOAuthTokenForAccount(
    const base::Value::ConstListView args) {
  // Two arguments. The first is callback id, and the second is the list
  // containing the account for which to fetch the oauth token.
  DCHECK_EQ(args.size(), 2u);

  const auto& requested_account = args[1];
  DCHECK(requested_account.is_list());
  DCHECK_EQ(requested_account.GetList().size(), 1u);

  auto& oauth_token_fetch_callback = args[0].GetString();
  const std::string& email = requested_account.GetList()[0].GetString();

  oauth_token_fetcher_.GetAccessTokenFor(
      email,
      base::BindOnce(&ProjectorMessageHandler::OnAccessTokenRequestCompleted,
                     GetWeakPtr(), oauth_token_fetch_callback));
}

void ProjectorMessageHandler::SendXhr(const base::Value::ConstListView args) {
  // Two arguments. The first is callback id, and the second is the list
  // containing function arguments for making the request.
  DCHECK_EQ(args.size(), 2u);
  const auto& callback_id = args[0].GetString();

  const auto& func_args = args[1].GetList();
  // Four function arguments:
  // 1. The request URL.
  // 2. The request method, for example: GET
  // 3. The request body data.
  // 4. A bool to indicate whether or not to use end user credential to
  // authorize the request.
  DCHECK_EQ(func_args.size(), 4u);

  const auto& url = func_args[0].GetString();
  const auto& method = func_args[1].GetString();
  std::string request_body = func_args[2].GetString();
  bool use_credentials = func_args[3].GetBool();
  DCHECK(!url.empty());
  DCHECK(!method.empty());

  xhr_sender_->Send(
      GURL(url), method, request_body, use_credentials,
      base::BindOnce(&ProjectorMessageHandler::OnXhrRequestCompleted,
                     GetWeakPtr(), callback_id));
}

void ProjectorMessageHandler::ShouldShowNewScreencastButton(
    const base::Value::ConstListView args) {
  AllowJavascript();
  // TODO(b/200205765): Add checks on whether new screencast button should be
  // shown.
  const auto& js_callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(js_callback_id), base::Value(false));
}

void ProjectorMessageHandler::ShouldDownloadSoda(
    const base::Value::ConstListView args) {
  AllowJavascript();
  // TODO(b/200205765): Add checks on whether the install soda button should be
  // shown.
  const auto& js_callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(js_callback_id), base::Value(false));
}

void ProjectorMessageHandler::InstallSoda(
    const base::Value::ConstListView args) {
  AllowJavascript();

  // TODO(b/200205765): Trigger SODA installation.
  const auto& js_callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(js_callback_id), base::Value(false));
}

void ProjectorMessageHandler::OnError(const base::Value::ConstListView args) {
  // TODO(b/195113693): Get the SWA dialog associated with this WebUI and close
  // it.
}

void ProjectorMessageHandler::OnAccessTokenRequestCompleted(
    const std::string& js_callback_id,
    const std::string& email,
    GoogleServiceAuthError error,
    const signin::AccessTokenInfo& info) {
  AllowJavascript();

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey(kUserEmail, base::Value(email));
  if (error.state() != GoogleServiceAuthError::State::NONE) {
    response.SetKey(kOAuthTokenInfo, base::Value());
    response.SetKey(kError, base::Value(ProjectorErrorToString(
                                ProjectorError::kTokenFetchFailure)));
  } else {
    response.SetKey(kError,
                    base::Value(ProjectorErrorToString(ProjectorError::kNone)));
    response.SetKey(kOAuthTokenInfo, AccessTokenInfoToValue(info));
  }

  ResolveJavascriptCallback(base::Value(js_callback_id), std::move(response));
}

void ProjectorMessageHandler::OnXhrRequestCompleted(
    const std::string& js_callback_id,
    bool success,
    const std::string& response_body,
    const std::string& error) {
  AllowJavascript();

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetBoolKey(kXhrSuccess, success);
  response.SetStringKey(kXhrResponseBody, response_body);
  response.SetStringKey(kXhrError, error);

  ResolveJavascriptCallback(base::Value(js_callback_id), std::move(response));
}

void ProjectorMessageHandler::GetPendingScreencasts(
    const base::Value::ConstListView args) {
  AllowJavascript();
  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);

  const std::set<PendingScreencast>& pending_screencasts =
      ProjectorAppClient::Get()->GetPendingScreencasts();
  ResolveJavascriptCallback(args[0],
                            ScreencastListToValue(pending_screencasts));
}

}  // namespace ash
