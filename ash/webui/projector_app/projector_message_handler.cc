// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_message_handler.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/projector_screencast.h"
#include "ash/webui/projector_app/projector_xhr_sender.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/web_ui.h"
#include "third_party/re2/src/re2/re2.h"
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

// Used when a request is rejected.
constexpr char kRejectedRequestMessage[] = "Request Rejected";
constexpr char kRejectedRequestMessageKey[] = "message";
constexpr char kRejectedRequestArgsKey[] = "requestArgs";

// Projector Error Strings.
constexpr char kNoneStr[] = "NONE";
constexpr char kOtherStr[] = "OTHER";
constexpr char kTokenFetchFailureStr[] = "TOKEN_FETCH_FAILURE";
// Disallow special chars that potentially allow redirecting writes to
// arbitrary file system locations.
constexpr char kInvalidStorageDirNameRegex[] = "\\.\\.|/|\\\\";

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

base::Value::List ScreencastListToValue(
    const PendingScreencastSet& screencasts) {
  base::Value::List value;
  value.reserve(screencasts.size());
  for (const auto& item : screencasts)
    value.Append(item.ToValue());

  return value;
}

bool IsUserPrefSupported(const std::string& pref) {
  return pref == ash::prefs::kProjectorCreationFlowEnabled ||
         pref == ash::prefs::kProjectorGalleryOnboardingShowCount ||
         pref == ash::prefs::kProjectorViewerOnboardingShowCount ||
         pref == ash::prefs::kProjectorExcludeTranscriptDialogShown;
}

bool IsValidOnboardingPref(const SetUserPrefArgs& args) {
  return args.value.is_int() &&
         (args.pref_name == ash::prefs::kProjectorGalleryOnboardingShowCount ||
          args.pref_name == ash::prefs::kProjectorViewerOnboardingShowCount);
}

bool IsValidCreationFlowPref(const SetUserPrefArgs& args) {
  return args.value.is_bool() &&
         args.pref_name == ash::prefs::kProjectorCreationFlowEnabled;
}

bool IsValidExcludeTranscriptDialogShownPref(const SetUserPrefArgs& args) {
  return args.value.is_bool() &&
         args.pref_name == ash::prefs::kProjectorExcludeTranscriptDialogShown;
}

bool IsValidPrefValueArg(const SetUserPrefArgs& args) {
  return IsValidCreationFlowPref(args) || IsValidOnboardingPref(args) ||
         IsValidExcludeTranscriptDialogShownPref(args);
}

// Returns true if the request, `args`, contains a valid user preference string.
// The `out` string is only valid if the function returns true.
bool GetUserPrefName(const base::Value& args, std::string* out) {
  if (!args.is_list())
    return false;

  const auto& args_list = args.GetList();

  if (args_list.size() != 1 || !args_list[0].is_string())
    return false;

  *out = args_list[0].GetString();

  return IsUserPrefSupported(*out);
}

// Returns true if the request, `args`, is valid and supported.
// The `out` struct is only valid if the function returns true.
bool GetSetUserPrefArgs(const base::Value& args, SetUserPrefArgs* out) {
  if (!args.is_list())
    return false;

  const auto& args_list = args.GetList();

  if (args_list.size() != 2 || !args_list[0].is_string()) {
    return false;
  }

  out->pref_name = args_list[0].GetString();
  out->value = args_list[1].Clone();
  return IsValidPrefValueArg(*out);
}

base::Value::Dict CreateRejectMessageForArgs(const base::Value& value) {
  base::Value::Dict rejected_response;
  rejected_response.Set(kRejectedRequestMessageKey, kRejectedRequestMessage);
  rejected_response.Set(kRejectedRequestArgsKey, value.Clone());
  return rejected_response;
}

}  // namespace

ProjectorMessageHandler::ProjectorMessageHandler(PrefService* pref_service)
    : content::WebUIMessageHandler(),
      xhr_sender_(std::make_unique<ProjectorXhrSender>(
          ProjectorAppClient::Get()->GetUrlLoaderFactory())),
      pref_service_(pref_service) {
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
      "getPendingScreencasts",
      base::BindRepeating(&ProjectorMessageHandler::GetPendingScreencasts,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getUserPref", base::BindRepeating(&ProjectorMessageHandler::GetUserPref,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setUserPref", base::BindRepeating(&ProjectorMessageHandler::SetUserPref,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openFeedbackDialog",
      base::BindRepeating(&ProjectorMessageHandler::OpenFeedbackDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getVideo", base::BindRepeating(&ProjectorMessageHandler::GetVideo,
                                      base::Unretained(this)));
}

void ProjectorMessageHandler::OnScreencastsPendingStatusChanged(
    const PendingScreencastSet& pending_screencast) {
  AllowJavascript();
  FireWebUIListener("onScreencastsStateChange",
                    ScreencastListToValue(pending_screencast));
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

void ProjectorMessageHandler::StartProjectorSession(
    const base::Value::List& args) {
  AllowJavascript();

  // There are two arguments. The first is the callback and the second is a list
  // containing the account which we need to start the recording with.
  DCHECK_EQ(args.size(), 2u);

  const auto& func_args = args[1];
  DCHECK(func_args.is_list());

  // The first entry is the drive directory to save the screen cast to.
  DCHECK_EQ(func_args.GetList().size(), 1u);
  auto storage_dir_name = func_args.GetList()[0].GetString();
  if (RE2::PartialMatch(storage_dir_name, kInvalidStorageDirNameRegex)) {
    ResolveJavascriptCallback(args[0], base::Value(false));
    return;
  }

  auto* controller = ProjectorController::Get();

  if (controller->GetNewScreencastPrecondition().state !=
      NewScreencastPreconditionState::kEnabled) {
    ResolveJavascriptCallback(args[0], base::Value(false));
    return;
  }

  controller->StartProjectorSession(storage_dir_name);
  ResolveJavascriptCallback(args[0], base::Value(true));
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

void ProjectorMessageHandler::SendXhr(const base::Value::List& args) {
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
  // 5. A bool to indicate whether or not to use api key to authorize the
  // request.
  // 6. Additional headers objects.
  // 7. The email address associated with the account
  DCHECK_EQ(func_args.size(), 7u);

  const auto& url = func_args[0].GetString();
  const auto& method = func_args[1].GetString();

  std::string request_body =
      func_args[2].is_string() ? func_args[2].GetString() : std::string();
  bool use_credentials =
      func_args[3].is_bool() ? func_args[3].GetBool() : false;
  bool use_api_key = func_args[4].is_bool() ? func_args[4].GetBool() : false;
  std::string account_email =
      func_args[6].is_string() ? func_args[6].GetString() : std::string();

  DCHECK(!url.empty());
  DCHECK(!method.empty());
  xhr_sender_->Send(
      GURL(url), method, request_body, use_credentials, use_api_key,
      base::BindOnce(&ProjectorMessageHandler::OnXhrRequestCompleted,
                     GetWeakPtr(), callback_id),
      func_args[5].is_dict() ? func_args[5].GetDict().Clone()
                             : base::Value::Dict(),
      account_email);
}

void ProjectorMessageHandler::OnError(const base::Value::List& args) {
  // TODO(b/195113693): Get the SWA dialog associated with this WebUI and close
  // it.
}

void ProjectorMessageHandler::GetUserPref(const base::Value::List& args) {
  AllowJavascript();

  std::string user_pref;
  if (!GetUserPrefName(args[1], &user_pref)) {
    RejectJavascriptCallback(args[0], CreateRejectMessageForArgs(args[1]));
    return;
  }

  ResolveJavascriptCallback(args[0], pref_service_->GetValue(user_pref));
}

void ProjectorMessageHandler::SetUserPref(const base::Value::List& args) {
  AllowJavascript();
  SetUserPrefArgs parsed_args;
  if (!GetSetUserPrefArgs(args[1], &parsed_args)) {
    RejectJavascriptCallback(args[0], CreateRejectMessageForArgs(args[1]));
    return;
  }

  pref_service_->Set(parsed_args.pref_name, parsed_args.value);
  ResolveJavascriptCallback(args[0], base::Value());
}

void ProjectorMessageHandler::OpenFeedbackDialog(
    const base::Value::List& args) {
  AllowJavascript();
  ProjectorAppClient::Get()->OpenFeedbackDialog();
  ResolveJavascriptCallback(args[0], base::Value());
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

void ProjectorMessageHandler::OnXhrRequestCompleted(
    const std::string& js_callback_id,
    bool success,
    const std::string& response_body,
    const std::string& error) {
  AllowJavascript();

  base::Value::Dict response;
  response.Set(kXhrSuccess, success);
  response.Set(kXhrResponseBody, response_body);
  response.Set(kXhrError, error);

  ResolveJavascriptCallback(base::Value(js_callback_id), response);
}

void ProjectorMessageHandler::GetPendingScreencasts(
    const base::Value::List& args) {
  AllowJavascript();
  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args.size(), 1u);

  const PendingScreencastSet& pending_screencasts =
      ProjectorAppClient::Get()->GetPendingScreencasts();
  ResolveJavascriptCallback(args[0],
                            ScreencastListToValue(pending_screencasts));
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
