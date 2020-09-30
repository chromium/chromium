// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_sharesheet.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "components/drive/drive_api_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/file_handlers/directory_util.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

using storage::FileSystemURL;

namespace extensions {

FileManagerPrivateInternalSharesheetHasTargetsFunction::
    FileManagerPrivateInternalSharesheetHasTargetsFunction()
    : chrome_details_(this) {}

FileManagerPrivateInternalSharesheetHasTargetsFunction::
    ~FileManagerPrivateInternalSharesheetHasTargetsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSharesheetHasTargetsFunction::Run() {
  using extensions::api::file_manager_private_internal::SharesheetHasTargets::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->urls.empty())
    return RespondNow(Error("No URLs provided"));

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  for (size_t i = 0; i < params->urls.size(); ++i) {
    const GURL url(params->urls[i]);
    storage::FileSystemURL file_system_url(file_system_context->CrackURL(url));
    if (drive::util::HasHostedDocumentExtension(file_system_url.path())) {
      contains_hosted_document_ = true;
    }
    if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
      continue;
    urls_.push_back(url);
    file_system_urls_.push_back(file_system_url);
  }

  mime_type_collector_ =
      std::make_unique<app_file_handler_util::MimeTypeCollector>(
          chrome_details_.GetProfile());
  mime_type_collector_->CollectForURLs(
      file_system_urls_,
      base::BindOnce(&FileManagerPrivateInternalSharesheetHasTargetsFunction::
                         OnMimeTypesCollected,
                     this));
  return RespondLater();
}

void FileManagerPrivateInternalSharesheetHasTargetsFunction::
    OnMimeTypesCollected(std::unique_ptr<std::vector<std::string>> mime_types) {
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(
          chrome_details_.GetProfile());

  bool result = false;

  if (!sharesheet_service) {
    LOG(ERROR) << "Couldn't get Sharesheet Service for profile";
    Respond(ArgumentList(extensions::api::file_manager_private_internal::
                             SharesheetHasTargets::Results::Create(result)));
    return;
  }

  if (file_system_urls_.size() == 1 &&
      file_system_urls_[0].type() == storage::kFileSystemTypeDriveFs) {
    auto connection_status = drive::util::GetDriveConnectionStatus(
        Profile::FromBrowserContext(browser_context()));

    if (connection_status == drive::util::DRIVE_CONNECTED_METERED ||
        connection_status == drive::util::DRIVE_CONNECTED) {
      file_manager::util::SingleEntryPropertiesGetterForDriveFs::Start(
          file_system_urls_[0], chrome_details_.GetProfile(),
          base::BindOnce(
              &FileManagerPrivateInternalSharesheetHasTargetsFunction::
                  OnDrivePropertyCollected,
              this, std::move(mime_types)));
      return;
    }
  }
  result = sharesheet_service->HasShareTargets(
      apps_util::CreateShareIntentFromFiles(urls_, *mime_types),
      contains_hosted_document_);
  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           SharesheetHasTargets::Results::Create(result)));
}

void FileManagerPrivateInternalSharesheetHasTargetsFunction::
    OnDrivePropertyCollected(
        std::unique_ptr<std::vector<std::string>> mime_types,
        std::unique_ptr<api::file_manager_private::EntryProperties> properties,
        base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Error reading file properties in Drive: " << error;
    Respond(ArgumentList(extensions::api::file_manager_private_internal::
                             SharesheetHasTargets::Results::Create(false)));
    return;
  }

  is_directory_collector_ =
      std::make_unique<app_file_handler_util::IsDirectoryCollector>(
          chrome_details_.GetProfile());
  is_directory_collector_->CollectForEntriesPaths(
      std::vector<base::FilePath>{file_system_urls_[0].path()},
      base::BindOnce(&FileManagerPrivateInternalSharesheetHasTargetsFunction::
                         OnIsDirectoryCollected,
                     this, std::move(mime_types), std::move(properties)));
}

void FileManagerPrivateInternalSharesheetHasTargetsFunction::
    OnIsDirectoryCollected(
        std::unique_ptr<std::vector<std::string>> mime_types,
        std::unique_ptr<api::file_manager_private::EntryProperties> properties,
        std::unique_ptr<std::set<base::FilePath>> path_directory_set) {
  bool is_directory = path_directory_set->find(file_system_urls_[0].path()) !=
                      path_directory_set->end();

  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(
          chrome_details_.GetProfile());
  GURL share_url =
      (properties->can_share && *properties->can_share && properties->share_url)
          ? GURL(*properties->share_url)
          : GURL();
  bool result = sharesheet_service->HasShareTargets(
      apps_util::CreateShareIntentFromDriveFile(urls_[0], (*mime_types)[0],
                                                share_url, is_directory),
      contains_hosted_document_);
  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           SharesheetHasTargets::Results::Create(result)));
}

FileManagerPrivateInternalInvokeSharesheetFunction::
    FileManagerPrivateInternalInvokeSharesheetFunction()
    : chrome_details_(this) {}

FileManagerPrivateInternalInvokeSharesheetFunction::
    ~FileManagerPrivateInternalInvokeSharesheetFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalInvokeSharesheetFunction::Run() {
  using extensions::api::file_manager_private_internal::InvokeSharesheet::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->urls.empty())
    return RespondNow(Error("No URLs provided"));

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details_.GetProfile(), render_frame_host());

  // Collect all the URLs, convert them to GURLs, and crack all the urls into
  // file paths.
  for (size_t i = 0; i < params->urls.size(); ++i) {
    const GURL url(params->urls[i]);
    storage::FileSystemURL file_system_url(file_system_context->CrackURL(url));
    if (drive::util::HasHostedDocumentExtension(file_system_url.path()))
      contains_hosted_document_ = true;
    if (!chromeos::FileSystemBackend::CanHandleURL(file_system_url))
      continue;
    urls_.push_back(url);
    file_system_urls_.push_back(file_system_url);
  }

  mime_type_collector_ =
      std::make_unique<app_file_handler_util::MimeTypeCollector>(
          chrome_details_.GetProfile());
  mime_type_collector_->CollectForURLs(
      file_system_urls_,
      base::BindOnce(&FileManagerPrivateInternalInvokeSharesheetFunction::
                         OnMimeTypesCollected,
                     this));

  return RespondLater();
}

void FileManagerPrivateInternalInvokeSharesheetFunction::OnMimeTypesCollected(
    std::unique_ptr<std::vector<std::string>> mime_types) {
  // On button press show sharesheet bubble.
  auto* profile = chrome_details_.GetProfile();
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile);
  if (!sharesheet_service) {
    Respond(Error("Cannot find sharesheet service"));
    return;
  }

  if (file_system_urls_.size() == 1 &&
      file_system_urls_[0].type() == storage::kFileSystemTypeDriveFs) {
    auto connection_status = drive::util::GetDriveConnectionStatus(
        Profile::FromBrowserContext(browser_context()));

    if (connection_status == drive::util::DRIVE_CONNECTED_METERED ||
        connection_status == drive::util::DRIVE_CONNECTED) {
      file_manager::util::SingleEntryPropertiesGetterForDriveFs::Start(
          file_system_urls_[0], chrome_details_.GetProfile(),
          base::BindOnce(&FileManagerPrivateInternalInvokeSharesheetFunction::
                             OnDrivePropertyCollected,
                         this, std::move(mime_types)));
      return;
    }
  }

  sharesheet_service->ShowBubble(
      GetSenderWebContents(),
      apps_util::CreateShareIntentFromFiles(urls_, *mime_types),
      contains_hosted_document_, base::NullCallback());
  Respond(NoArguments());
}

void FileManagerPrivateInternalInvokeSharesheetFunction::
    OnDrivePropertyCollected(
        std::unique_ptr<std::vector<std::string>> mime_types,
        std::unique_ptr<api::file_manager_private::EntryProperties> properties,
        base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error != base::File::FILE_OK) {
    Respond(Error("Drive File Error"));
    return;
  }

  is_directory_collector_ =
      std::make_unique<app_file_handler_util::IsDirectoryCollector>(
          chrome_details_.GetProfile());
  is_directory_collector_->CollectForEntriesPaths(
      std::vector<base::FilePath>{file_system_urls_[0].path()},
      base::BindOnce(&FileManagerPrivateInternalInvokeSharesheetFunction::
                         OnIsDirectoryCollected,
                     this, std::move(mime_types), std::move(properties)));
}

void FileManagerPrivateInternalInvokeSharesheetFunction::OnIsDirectoryCollected(
    std::unique_ptr<std::vector<std::string>> mime_types,
    std::unique_ptr<api::file_manager_private::EntryProperties> properties,
    std::unique_ptr<std::set<base::FilePath>> path_directory_set) {
  bool is_directory = path_directory_set->find(file_system_urls_[0].path()) !=
                      path_directory_set->end();

  auto* profile = chrome_details_.GetProfile();
  sharesheet::SharesheetService* sharesheet_service =
      sharesheet::SharesheetServiceFactory::GetForProfile(profile);
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
      apps_util::CreateShareIntentFromDriveFile(urls_[0], (*mime_types)[0],
                                                share_url, is_directory),
      contains_hosted_document_, base::NullCallback());
  Respond(NoArguments());
}

}  // namespace extensions
