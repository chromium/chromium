// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_confirmation_modal.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/download_item_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

gfx::NativeWindow FindMostRelevantContextWindow(
    const content::WebContents* web_contents) {
  // Can't just use web_contents->GetNativeView(): it results in a dialog that
  // disappears upon browser going out of focus and cannot be re-activated or
  // closed by user.
  auto* browser =
      web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;

  // Back up methods are needed to find a window to attach the dialog to,
  // because the |web_contents| from |download_item| is stored as a mapping
  // inside of it and is not guaranteed to always exist or be valid. Example: if
  // the original window got closed when download was still in progress; or if
  // we need to resume file upload upon browser restart.
  if (!browser) {
    LOG(ERROR) << "Can't find window from download item; using active window";
    browser = chrome::FindBrowserWithActiveWindow();
  }
  if (!browser) {
    LOG(ERROR) << "Can't find active window; using last active window";
    browser = chrome::FindLastActive();
  }
  DCHECK(browser);
  return browser->window()->GetNativeWindow();
}

namespace ec = enterprise_connectors;

bool MimeTypeMatches(const std::set<std::string>& mime_types_to_match,
                     const std::string& mime_type) {
  for (const std::string& mime_type_pattern : mime_types_to_match) {
    if (net::MatchesMimeType(mime_type_pattern, mime_type)) {
      return true;
    }
  }
  return false;
}

ec::ConnectorsService* GetConnectorsService(content::BrowserContext* context) {
  if (!base::FeatureList::IsEnabled(ec::kFileSystemConnectorEnabled))
    return nullptr;

  // Check to see if the download item matches any rules.  If the URL of the
  // download itself does not match then check the URL of site on which the
  // download is hosted.
  DCHECK(context);
  return ec::ConnectorsServiceFactory::GetForBrowserContext(context);
}

}  // namespace

namespace enterprise_connectors {

absl::optional<FileSystemSettings> GetFileSystemSettings(Profile* profile) {
  auto* service = GetConnectorsService(profile);
  if (!service)
    return absl::nullopt;
  return service->GetFileSystemGlobalSettings(
      FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
}

absl::optional<FileSystemSettings> MatchedToEnable(ConnectorsService* service,
                                                   const GURL& url,
                                                   std::string mime_type) {
  absl::optional<FileSystemSettings> settings = service->GetFileSystemSettings(
      url, FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
  if (!settings.has_value())
    return absl::nullopt;

  bool mime_matched = MimeTypeMatches(settings->mime_types, mime_type);

  // The condition mime_matched == settings->enable_with_mime_types includes 2
  // cases that should result in the decision to enable routing:
  //    1/ Did match and settings->mime_types was for enabling;
  //    2/ Didn't match but settings->mime_types was for disabling.
  return (mime_matched == settings->enable_with_mime_types) ? settings
                                                            : absl::nullopt;
}

absl::optional<FileSystemSettings> GetFileSystemSettings(
    download::DownloadItem* download_item) {
  auto* context = content::DownloadItemUtils::GetBrowserContext(download_item);
  auto* service = GetConnectorsService(context);
  if (!service)
    return absl::nullopt;

  absl::optional<FileSystemSettings> settings = MatchedToEnable(
      service, download_item->GetURL(), download_item->GetMimeType());
  if (settings.has_value())
    return settings;

  return MatchedToEnable(service, download_item->GetTabUrl(),
                         download_item->GetMimeType());
}

void OnConfirmationModalClosed(gfx::NativeWindow context,
                               content::BrowserContext* browser_context,
                               const FileSystemSettings& settings,
                               PrefService* prefs,
                               AuthorizationCompletedCallback callback,
                               SigninExperienceTestObserver* test_observer,
                               bool user_confirmed_to_proceed) {
  if (!user_confirmed_to_proceed) {
    return ReturnCancellation(std::move(callback));
  }

  auto account_info = GetFileSystemAccountInfoFromPrefs(settings, prefs);
  auto account_login = account_info
                           ? absl::make_optional(account_info->account_login)
                           : absl::nullopt;
  std::unique_ptr<FileSystemSigninDialogDelegate> delegate =
      std::make_unique<FileSystemSigninDialogDelegate>(
          browser_context, settings, std::move(account_login),
          std::move(callback));
  content::WebContents* dialog_web_contents = delegate->web_contents();
  FileSystemSigninDialogDelegate* delegate_ptr = delegate.get();

  // We want a dialog whose lifetime is independent from that of |web_contents|,
  // therefore using FindMostRelevantContextWindow() as context, instead of
  // using web_contents->GetNativeView() as parent. This gives us a new
  // top-level window.
  auto* widget = views::DialogDelegate::CreateDialogWidget(
      std::move(delegate), context, /* parent = */ nullptr);

  if (test_observer)
    test_observer->OnSignInDialogCreated(dialog_web_contents, delegate_ptr,
                                         widget);

  widget->Show();
}

// Start the sign in experience as triggered by a download item.
void StartFileSystemConnectorSigninExperienceForDownloadItem(
    content::WebContents* web_contents,
    const FileSystemSettings& settings,
    PrefService* prefs,
    AuthorizationCompletedCallback callback,
    SigninExperienceTestObserver* test_observer) {
  gfx::NativeWindow context = FindMostRelevantContextWindow(web_contents);
  DCHECK(context);

  DCHECK_EQ(settings.service_provider, kFileSystemServiceProviderPrefNameBox);
  std::u16string provider =
      l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX);

  base::OnceCallback<void(bool)> confirmed_to_sign_in = base::BindOnce(
      &OnConfirmationModalClosed, context, web_contents->GetBrowserContext(),
      settings, prefs, std::move(callback), test_observer);
  FileSystemConfirmationModal::Show(
      context,
      l10n_util::GetStringFUTF16(
          IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_REQUIRED_TITLE, provider),
      l10n_util::GetStringFUTF16(
          IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_REQUIRED_MESSAGE, provider),
      l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_REQUIRED_CANCEL_BUTTON),
      l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_REQUIRED_ACCEPT_BUTTON),
      std::move(confirmed_to_sign_in), test_observer);
}

void OnConfirmationModalClosedForSettingsPage(
    gfx::NativeWindow context,
    Profile* profile,
    const FileSystemSettings& settings,
    base::OnceCallback<void(bool)> settings_page_callback,
    SigninExperienceTestObserver* test_observer,
    bool user_confirmed_to_proceed) {
  AuthorizationCompletedCallback converted_cb = base::BindOnce(
      [](PrefService* prefs, const std::string& provider,
         base::OnceCallback<void(bool)> cb,
         const GoogleServiceAuthError& status, const std::string& access_token,
         const std::string& refresh_token) {
        bool signin_success =
            (status.state() == GoogleServiceAuthError::State::NONE);
        if (signin_success) {
          SetFileSystemOAuth2Tokens(prefs, provider, access_token,
                                    refresh_token);
        }
        std::move(cb).Run(signin_success);
      },
      profile->GetPrefs(), settings.service_provider,
      std::move(settings_page_callback));
  OnConfirmationModalClosed(context, profile, settings, profile->GetPrefs(),
                            std::move(converted_cb), test_observer,
                            user_confirmed_to_proceed);
}

// Start the sign in experience as triggered by the settings page. Similar to
// StartFileSystemConnectorSigninExperienceForDownloadItem() but with different
// displayed texts for FileSystemConfirmationModal::Show().
void StartFileSystemConnectorSigninExperienceForSettingsPage(
    Profile* profile,
    base::OnceCallback<void(bool)> callback,
    SigninExperienceTestObserver* test_observer) {
  gfx::NativeWindow context = FindMostRelevantContextWindow(nullptr);
  DCHECK(context);

  auto settings = GetFileSystemSettings(profile);
  if (!settings.has_value())
    return std::move(callback).Run(false);

  DCHECK_EQ(settings->service_provider, kFileSystemServiceProviderPrefNameBox);
  std::u16string provider =
      l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX);

  base::OnceCallback<void(bool)> confirmed_to_sign_in = base::BindOnce(
      &OnConfirmationModalClosedForSettingsPage, context, profile,
      settings.value(), std::move(callback), test_observer);
  FileSystemConfirmationModal::Show(
      context,
      l10n_util::GetStringFUTF16(IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_CONFIRM_TITLE,
                                 provider),
      l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_CONFIRM_MESSAGE),
      l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_CONFIRM_CANCEL_BUTTON),
      l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_CONNECTOR_SIGNIN_CONFIRM_ACCEPT_BUTTON),
      std::move(confirmed_to_sign_in));
}

// Clear authentication tokens and stored account info.
bool ClearFileSystemConnectorLinkedAccount(const FileSystemSettings& settings,
                                           PrefService* prefs) {
  ClearDefaultFolder(prefs, settings.service_provider);
  return ClearFileSystemOAuth2Tokens(prefs, settings.service_provider) &&
         ClearFileSystemAccountInfo(prefs, settings.service_provider);
}

std::vector<std::string> GetFileSystemConnectorPrefsForSettingsPage(
    Profile* profile) {
  std::vector<std::string> prefs_paths;
  auto settings = GetFileSystemSettings(profile);
  if (settings.has_value()) {
    return GetFileSystemConnectorAccountInfoPrefs(settings->service_provider);
  }
  return std::vector<std::string>();
}

absl::optional<AccountInfo> GetFileSystemConnectorLinkedAccountInfo(
    const FileSystemSettings& settings,
    PrefService* prefs) {
  const std::string& provider = settings.service_provider;
  std::string refresh_token;
  if (!GetFileSystemOAuth2Tokens(prefs, provider, /* access_token = */ nullptr,
                                 &refresh_token) ||
      refresh_token.empty()) {
    return absl::nullopt;
  }

  return GetFileSystemAccountInfoFromPrefs(settings, prefs);
}

void SetFileSystemConnectorAccountLinkForSettingsPage(
    bool enable_link,
    Profile* profile,
    base::OnceCallback<void(bool)> callback,
    SigninExperienceTestObserver* test_observer) {
  absl::optional<FileSystemSettings> settings = GetFileSystemSettings(profile);
  auto has_linked_account =
      settings.has_value() && GetFileSystemConnectorLinkedAccountInfo(
                                  settings.value(), profile->GetPrefs());

  // Early return if linked state already match the desired state.
  if (has_linked_account == enable_link) {
    std::move(callback).Run(true);
    return;
  }

  // Early return after a quick clearing function call.
  if (has_linked_account) {
    bool success = ClearFileSystemConnectorLinkedAccount(
        GetFileSystemSettings(profile).value(), profile->GetPrefs());
    std::move(callback).Run(success);
    return;
  }

  // This shows dialogs for the sign-in experience that the user needs to
  // interact with, so the process is async.
  StartFileSystemConnectorSigninExperienceForSettingsPage(
      profile, std::move(callback), test_observer);
}

void ReturnCancellation(AuthorizationCompletedCallback callback) {
  std::move(callback).Run(
      GoogleServiceAuthError{GoogleServiceAuthError::State::REQUEST_CANCELED},
      std::string(), std::string());
}

// SigninExperienceTestObserver
SigninExperienceTestObserver::SigninExperienceTestObserver() = default;

void SigninExperienceTestObserver::InitForTesting(
    FileSystemRenameHandler* rename_handler) {
  if (!rename_handler)
    return;
  rename_handler_ =
      rename_handler->RegisterSigninObserverForTesting(this);  // IN-TEST
}

SigninExperienceTestObserver::~SigninExperienceTestObserver() {
  if (!rename_handler_)
    return;
  rename_handler_->UnregisterSigninObserverForTesting(this);  // IN-TEST
  rename_handler_.reset();
}

// TODO(https://crbug.com/1159185): add browser_tests for
// StartFileSystemConnectorSigninExperienceForXxx.

}  // namespace enterprise_connectors
