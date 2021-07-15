// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/service_settings.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_confirmation_modal.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/download_item_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"
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

bool MimeTypeMatches(const std::set<std::string>& mime_types,
                     const std::string& mime_type) {
  return mime_types.count(ec::kWildcardMimeType) != 0 ||
         mime_types.count(mime_type) != 0;
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

absl::optional<FileSystemSettings> GetFileSystemSettings(
    download::DownloadItem* download_item) {
  auto* context = content::DownloadItemUtils::GetBrowserContext(download_item);
  auto* service = GetConnectorsService(context);
  if (!service)
    return absl::nullopt;

  auto settings = service->GetFileSystemSettings(
      download_item->GetURL(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
  if (settings.has_value() &&
      MimeTypeMatches(settings->mime_types, download_item->GetMimeType())) {
    return settings;
  }

  settings = service->GetFileSystemSettings(
      download_item->GetTabUrl(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
  if (settings.has_value() &&
      MimeTypeMatches(settings->mime_types, download_item->GetMimeType())) {
    return settings;
  }

  return absl::nullopt;
}

void OnConfirmationModalClosed(gfx::NativeWindow context,
                               content::BrowserContext* browser_context,
                               const FileSystemSettings& settings,
                               AuthorizationCompletedCallback callback,
                               bool user_confirmed_to_proceed) {
  if (!user_confirmed_to_proceed) {
    return ReturnCancellation(std::move(callback));
  }

  std::unique_ptr<FileSystemSigninDialogDelegate> delegate =
      std::make_unique<FileSystemSigninDialogDelegate>(
          browser_context, settings, std::move(callback));

  // We want a dialog whose lifetime is independent from that of |web_contents|,
  // therefore using FindMostRelevantContextWindow() as context, instead of
  // using web_contents->GetNativeView() as parent. This gives us a new
  // top-level window.
  auto* widget = views::DialogDelegate::CreateDialogWidget(
      std::move(delegate), context, /* parent = */ nullptr);
  widget->Show();
}

void StartFileSystemConnectorSigninExperienceForDownloadItem(
    content::WebContents* web_contents,
    const FileSystemSettings& settings,
    AuthorizationCompletedCallback callback) {
  gfx::NativeWindow context = FindMostRelevantContextWindow(web_contents);
  DCHECK(context);

  DCHECK_EQ(settings.service_provider, kBoxProviderName);
  std::u16string provider =
      l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX);

  base::OnceCallback<void(bool)> confirmed_to_sign_in = base::BindOnce(
      &OnConfirmationModalClosed, context, web_contents->GetBrowserContext(),
      settings, std::move(callback));
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
      std::move(confirmed_to_sign_in));
}

void ReturnCancellation(AuthorizationCompletedCallback callback) {
  std::move(callback).Run(
      GoogleServiceAuthError{GoogleServiceAuthError::State::REQUEST_CANCELED},
      std::string(), std::string());
}

}  // namespace enterprise_connectors
