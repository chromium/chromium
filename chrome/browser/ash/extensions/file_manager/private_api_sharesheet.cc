// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_sharesheet.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "components/drive/drive_api_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

using storage::FileSystemURL;

namespace {

using extensions::api::file_manager_private::SharesheetLaunchSource;

sharesheet::LaunchSource GetLaunchSource(SharesheetLaunchSource launch_source) {
  switch (launch_source) {
    case (SharesheetLaunchSource::kSharesheetButton):
      return sharesheet::LaunchSource::kFilesAppShareButton;
    case (SharesheetLaunchSource::kContextMenu):
      return sharesheet::LaunchSource::kFilesAppContextMenu;
    case (SharesheetLaunchSource::kUnknown):
    case (SharesheetLaunchSource::kNone):
      return sharesheet::LaunchSource::kUnknown;
  }
}

}  // namespace

namespace extensions {

FileManagerPrivateSharesheetHasTargetsFunction::
    FileManagerPrivateSharesheetHasTargetsFunction() = default;

FileManagerPrivateSharesheetHasTargetsFunction::
    ~FileManagerPrivateSharesheetHasTargetsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateSharesheetHasTargetsFunction::Run() {
  using extensions::api::file_manager_private::SharesheetHasTargets::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->file_urls.empty()) {
    return RespondNow(Error("No URLs provided"));
  }

  profile_ = Profile::FromBrowserContext(browser_context());

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile_, render_frame_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  for (const auto& url_as_string : params->file_urls) {
    const GURL url(url_as_string);
    storage::FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(url));
    if (!ash::FileSystemBackend::CanHandleURL(file_system_url)) {
      continue;
    }
    urls_.push_back(url);
    file_system_urls_.push_back(file_system_url);
  }

  mime_type_collector_ =
      std::make_unique<app_file_handler_util::MimeTypeCollector>(profile_);
  mime_type_collector_->CollectForURLs(
      file_system_urls_,
      base::BindOnce(
          &FileManagerPrivateSharesheetHasTargetsFunction::OnMimeTypesCollected,
          this));
  return RespondLater();
}

void FileManagerPrivateSharesheetHasTargetsFunction::OnMimeTypesCollected(
    std::unique_ptr<std::vector<std::string>> mime_types) {
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);

  bool result = false;

  if (!sharesheet_service) {
    LOG(ERROR) << "Couldn't get Sharesheet Service for profile";
    Respond(ArgumentList(extensions::api::file_manager_private::
                             SharesheetHasTargets::Results::Create(result)));
    return;
  }

  if (file_system_urls_.size() == 1 &&
      file_system_urls_[0].type() == storage::kFileSystemTypeDriveFs) {
    using drive::util::ConnectionStatus;
    const ConnectionStatus status = drive::util::GetDriveConnectionStatus(
        Profile::FromBrowserContext(browser_context()));

    using enum ConnectionStatus;
    if (status == kMetered || status == kConnected) {
      file_manager::util::SingleEntryPropertiesGetterForDriveFs::Start(
          file_system_urls_[0], profile_,
          base::BindOnce(&FileManagerPrivateSharesheetHasTargetsFunction::
                             OnDrivePropertyCollected,
                         this, std::move(mime_types)));
      return;
    }
  }
  result = sharesheet_service->HasShareTargets(
      apps_util::MakeShareIntent(urls_, *mime_types));
  Respond(ArgumentList(extensions::api::file_manager_private::
                           SharesheetHasTargets::Results::Create(result)));
}

void FileManagerPrivateSharesheetHasTargetsFunction::OnDrivePropertyCollected(
    std::unique_ptr<std::vector<std::string>> mime_types,
    std::unique_ptr<api::file_manager_private::EntryProperties> properties,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Error reading file properties in Drive: " << error;
    Respond(ArgumentList(extensions::api::file_manager_private::
                             SharesheetHasTargets::Results::Create(false)));
    return;
  }

  is_directory_collector_ =
      std::make_unique<app_file_handler_util::IsDirectoryCollector>(profile_);
  is_directory_collector_->CollectForEntriesPaths(
      std::vector<base::FilePath>{file_system_urls_[0].path()},
      base::BindOnce(&FileManagerPrivateSharesheetHasTargetsFunction::
                         OnIsDirectoryCollected,
                     this, std::move(mime_types), std::move(properties)));
}

void FileManagerPrivateSharesheetHasTargetsFunction::OnIsDirectoryCollected(
    std::unique_ptr<std::vector<std::string>> mime_types,
    std::unique_ptr<api::file_manager_private::EntryProperties> properties,
    std::unique_ptr<std::set<base::FilePath>> path_directory_set) {
  bool is_directory = path_directory_set->find(file_system_urls_[0].path()) !=
                      path_directory_set->end();

  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  GURL share_url =
      (properties->can_share && *properties->can_share && properties->share_url)
          ? GURL(*properties->share_url)
          : GURL();
  bool result = sharesheet_service->HasShareTargets(apps_util::MakeShareIntent(
      urls_[0], (*mime_types)[0], share_url, is_directory));
  Respond(ArgumentList(extensions::api::file_manager_private::
                           SharesheetHasTargets::Results::Create(result)));
}

FileManagerPrivateInvokeSharesheetFunction::
    FileManagerPrivateInvokeSharesheetFunction() = default;

FileManagerPrivateInvokeSharesheetFunction::
    ~FileManagerPrivateInvokeSharesheetFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInvokeSharesheetFunction::Run() {
  using extensions::api::file_manager_private::InvokeSharesheet::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->file_urls.empty()) {
    return RespondNow(Error("No URLs provided"));
  }

  if (params->dlp_source_urls.size() != params->file_urls.size()) {
    return RespondNow(Error("Mismatching URLs and DLP source URLs provided"));
  }

  profile_ = Profile::FromBrowserContext(browser_context());

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile_, render_frame_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  for (const auto& url_string : params->file_urls) {
    const GURL url(url_string);
    storage::FileSystemURL file_system_url(
        file_system_context->CrackURLInFirstPartyContext(url));
    if (!ash::FileSystemBackend::CanHandleURL(file_system_url)) {
      continue;
    }
    urls_.push_back(url);
    file_system_urls_.push_back(file_system_url);
  }

  dlp_source_urls_ = std::move(params->dlp_source_urls);

  mime_type_collector_ =
      std::make_unique<app_file_handler_util::MimeTypeCollector>(profile_);
  mime_type_collector_->CollectForURLs(
      file_system_urls_,
      base::BindOnce(
          &FileManagerPrivateInvokeSharesheetFunction::OnMimeTypesCollected,
          this, GetLaunchSource(params->launch_source)));

  return RespondLater();
}

void FileManagerPrivateInvokeSharesheetFunction::OnMimeTypesCollected(
    sharesheet::LaunchSource launch_source,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  // On button press show sharesheet bubble.
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  if (!sharesheet_service) {
    Respond(Error("Cannot find sharesheet service"));
    return;
  }

  if (file_system_urls_.size() == 1 &&
      file_system_urls_[0].type() == storage::kFileSystemTypeDriveFs) {
    using drive::util::ConnectionStatus;
    const ConnectionStatus status = drive::util::GetDriveConnectionStatus(
        Profile::FromBrowserContext(browser_context()));

    using enum ConnectionStatus;
    if (status == kMetered || status == kConnected) {
      file_manager::util::SingleEntryPropertiesGetterForDriveFs::Start(
          file_system_urls_[0], profile_,
          base::BindOnce(&FileManagerPrivateInvokeSharesheetFunction::
                             OnDrivePropertyCollected,
                         this, launch_source, std::move(mime_types)));
      return;
    }
  }

  sharesheet_service->ShowBubble(
      GetSenderWebContents(),
      apps_util::MakeShareIntent(urls_, *mime_types, dlp_source_urls_),
      launch_source, base::DoNothing());
  Respond(NoArguments());
}

void FileManagerPrivateInvokeSharesheetFunction::OnDrivePropertyCollected(
    sharesheet::LaunchSource launch_source,
    std::unique_ptr<std::vector<std::string>> mime_types,
    std::unique_ptr<api::file_manager_private::EntryProperties> properties,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error != base::File::FILE_OK) {
    Respond(Error("Drive File Error"));
    return;
  }

  is_directory_collector_ =
      std::make_unique<app_file_handler_util::IsDirectoryCollector>(profile_);
  is_directory_collector_->CollectForEntriesPaths(
      std::vector<base::FilePath>{file_system_urls_[0].path()},
      base::BindOnce(
          &FileManagerPrivateInvokeSharesheetFunction::OnIsDirectoryCollected,
          this, launch_source, std::move(mime_types), std::move(properties)));
}

void FileManagerPrivateInvokeSharesheetFunction::OnIsDirectoryCollected(
    sharesheet::LaunchSource launch_source,
    std::unique_ptr<std::vector<std::string>> mime_types,
    std::unique_ptr<api::file_manager_private::EntryProperties> properties,
    std::unique_ptr<std::set<base::FilePath>> path_directory_set) {
  bool is_directory = path_directory_set->find(file_system_urls_[0].path()) !=
                      path_directory_set->end();

  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile_);
  if (!sharesheet_service) {
    Respond(Error("Cannot find sharesheet service"));
    return;
  }

  GURL share_url =
      (properties->can_share && *properties->can_share && properties->share_url)
          ? GURL(*properties->share_url)
          : GURL();
  sharesheet_service->ShowBubble(
      GetSenderWebContents(),
      apps_util::MakeShareIntent(urls_[0], (*mime_types)[0], share_url,
                                 is_directory),
      launch_source, base::DoNothing());
  Respond(NoArguments());
}

}  // namespace extensions
