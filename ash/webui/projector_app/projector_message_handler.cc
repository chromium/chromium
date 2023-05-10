// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_message_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_screencast.h"
#include "base/check.h"
#include "base/functional/bind.h"
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

// Projector Error Strings.
constexpr char kNoneStr[] = "NONE";
constexpr char kOtherStr[] = "OTHER";
constexpr char kTokenFetchFailureStr[] = "TOKEN_FETCH_FAILURE";

// Struct used to describe args to set user's preference.
struct SetUserPrefArgs {
  std::string pref_name;
  base::Value value;
};

base::Value::Dict AccessTokenInfoToValue(const signin::AccessTokenInfo& info) {
  base::Value::Dict value;
  value.Set(kToken, info.token);
  value.Set(kExpirationTime, base::TimeToValue(info.expiration_time));
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

}  // namespace

ProjectorMessageHandler::ProjectorMessageHandler() = default;
ProjectorMessageHandler::~ProjectorMessageHandler() = default;

base::WeakPtr<ProjectorMessageHandler> ProjectorMessageHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProjectorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAccounts", base::BindRepeating(&ProjectorMessageHandler::GetAccounts,
                                         base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getOAuthTokenForAccount",
      base::BindRepeating(&ProjectorMessageHandler::GetOAuthTokenForAccount,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "onError", base::BindRepeating(&ProjectorMessageHandler::OnError,
                                     base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getVideo", base::BindRepeating(&ProjectorMessageHandler::GetVideo,
                                      base::Unretained(this)));
}

void ProjectorMessageHandler::GetAccounts(const base::Value::List& args) {
  AllowJavascript();

  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);

  const std::vector<AccountInfo> accounts = oauth_token_fetcher_.GetAccounts();
  const CoreAccountInfo primary_account =
      oauth_token_fetcher_.GetPrimaryAccountInfo();

  base::Value::List response;
  response.reserve(accounts.size());
  for (const auto& info : accounts) {
    base::Value::Dict account_info;
    account_info.Set(kUserName, info.full_name);
    account_info.Set(kUserEmail, info.email);
    account_info.Set(kUserPictureURL, info.picture_url);
    account_info.Set(kIsPrimaryUser, info.gaia == primary_account.gaia);
    response.Append(std::move(account_info));
  }

  ResolveJavascriptCallback(args[0], response);
}

void ProjectorMessageHandler::GetOAuthTokenForAccount(
    const base::Value::List& args) {
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

void ProjectorMessageHandler::OnError(const base::Value::List& args) {
  // TODO(b/195113693): Get the SWA dialog associated with this WebUI and close
  // it.
}

void ProjectorMessageHandler::OnAccessTokenRequestCompleted(
    const std::string& js_callback_id,
    const std::string& email,
    GoogleServiceAuthError error,
    const signin::AccessTokenInfo& info) {
  AllowJavascript();

  base::Value::Dict response;
  response.Set(kUserEmail, base::Value(email));
  if (error.state() != GoogleServiceAuthError::State::NONE) {
    response.Set(kOAuthTokenInfo, base::Value());
    response.Set(kError, base::Value(ProjectorErrorToString(
                             ProjectorError::kTokenFetchFailure)));
  } else {
    response.Set(kError,
                 base::Value(ProjectorErrorToString(ProjectorError::kNone)));
    response.Set(kOAuthTokenInfo, AccessTokenInfoToValue(info));
  }

  ResolveJavascriptCallback(base::Value(js_callback_id), response);
}

void ProjectorMessageHandler::GetVideo(const base::Value::List& args) {
  // Two arguments. The first is callback id, and the second is the list
  // containing the item id and resource key.
  DCHECK_EQ(args.size(), 2u);
  const auto& func_args = args[1].GetList();
  DCHECK_EQ(func_args.size(), 2u);

  const std::string& js_callback_id = args[0].GetString();
  const std::string& video_file_id = func_args[0].GetString();
  std::string resource_key;
  if (func_args[1].is_string())
    resource_key = func_args[1].GetString();

  ProjectorAppClient::Get()->GetVideo(
      video_file_id, resource_key,
      base::BindOnce(&ProjectorMessageHandler::OnVideoLocated, GetWeakPtr(),
                     js_callback_id));
}

void ProjectorMessageHandler::OnVideoLocated(
    const std::string& js_callback_id,
    std::unique_ptr<ProjectorScreencastVideo> video,
    const std::string& error_message) {
  AllowJavascript();

  if (!error_message.empty()) {
    RejectJavascriptCallback(base::Value(js_callback_id),
                             base::Value(error_message));
    return;
  }
  DCHECK(video)
      << "If there is no error message, then video should not be nullptr";
  ResolveJavascriptCallback(base::Value(js_callback_id), video->ToValue());
}
}  // namespace ash
