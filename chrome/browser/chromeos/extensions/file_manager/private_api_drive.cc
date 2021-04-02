// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/string_search.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drivefs_native_message_host.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/components/drivefs/drivefs_util.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/drive/chromeos/search_metadata.h"
#include "components/drive/event_logger.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/gurl.h"

using content::BrowserThread;

using chromeos::file_system_provider::EntryMetadata;
using chromeos::file_system_provider::ProvidedFileSystemInterface;
using chromeos::file_system_provider::util::FileSystemURLParser;
using extensions::api::file_manager_private::EntryProperties;
using extensions::api::file_manager_private::EntryPropertyName;
using file_manager::util::EntryDefinition;
using file_manager::util::EntryDefinitionCallback;
using file_manager::util::EntryDefinitionList;
using file_manager::util::EntryDefinitionListCallback;
using file_manager::util::FileDefinition;
using file_manager::util::FileDefinitionList;
using google_apis::DriveApiUrlGenerator;

namespace extensions {
namespace {

constexpr char kAvailableOfflinePropertyName[] = "availableOffline";

// Thresholds for logging slow operations.
constexpr base::TimeDelta kDriveSlowOperationThreshold =
    base::TimeDelta::FromSeconds(5);
constexpr base::TimeDelta kDriveVerySlowOperationThreshold =
    base::TimeDelta::FromMinutes(1);

class SingleEntryPropertiesGetterForFileSystemProvider {
 public:
  typedef base::OnceCallback<void(std::unique_ptr<EntryProperties> properties,
                                  base::File::Error error)>
      ResultCallback;

  // Creates an instance and starts the process.
  static void Start(const storage::FileSystemURL file_system_url,
                    const std::set<EntryPropertyName>& names,
                    ResultCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    SingleEntryPropertiesGetterForFileSystemProvider* instance =
        new SingleEntryPropertiesGetterForFileSystemProvider(
            file_system_url, names, std::move(callback));
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

  virtual ~SingleEntryPropertiesGetterForFileSystemProvider() = default;

 private:
  SingleEntryPropertiesGetterForFileSystemProvider(
      const storage::FileSystemURL& file_system_url,
      const std::set<EntryPropertyName>& names,
      ResultCallback callback)
      : callback_(std::move(callback)),
        file_system_url_(file_system_url),
        names_(names),
        properties_(new EntryProperties) {
    DCHECK(!callback_.is_null());
  }

  void StartProcess() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    FileSystemURLParser parser(file_system_url_);
    if (!parser.Parse()) {
      CompleteGetEntryProperties(base::File::FILE_ERROR_NOT_FOUND);
      return;
    }

    ProvidedFileSystemInterface::MetadataFieldMask field_mask =
        ProvidedFileSystemInterface::METADATA_FIELD_NONE;
    if (names_.find(api::file_manager_private::ENTRY_PROPERTY_NAME_SIZE) !=
        names_.end()) {
      field_mask |= ProvidedFileSystemInterface::METADATA_FIELD_SIZE;
    }
    if (names_.find(
            api::file_manager_private::ENTRY_PROPERTY_NAME_MODIFICATIONTIME) !=
        names_.end()) {
      field_mask |=
          ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME;
    }
    if (names_.find(
            api::file_manager_private::ENTRY_PROPERTY_NAME_CONTENTMIMETYPE) !=
        names_.end()) {
      field_mask |= ProvidedFileSystemInterface::METADATA_FIELD_MIME_TYPE;
    }
    if (names_.find(
            api::file_manager_private::ENTRY_PROPERTY_NAME_THUMBNAILURL) !=
        names_.end()) {
      field_mask |= ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL;
    }
    if (!field_mask) {
      OnGetMetadataCompleted(nullptr, base::File::FILE_OK);
      return;
    }

    parser.file_system()->GetMetadata(
        parser.file_path(), field_mask,
        base::BindOnce(&SingleEntryPropertiesGetterForFileSystemProvider::
                           OnGetMetadataCompleted,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetMetadataCompleted(std::unique_ptr<EntryMetadata> metadata,
                              base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (result != base::File::FILE_OK) {
      CompleteGetEntryProperties(result);
      return;
    }

    if (names_.find(api::file_manager_private::ENTRY_PROPERTY_NAME_SIZE) !=
        names_.end()) {
      properties_->size = std::make_unique<double>(*metadata->size.get());
    }

    if (names_.find(
            api::file_manager_private::ENTRY_PROPERTY_NAME_MODIFICATIONTIME) !=
        names_.end()) {
      properties_->modification_time =
          std::make_unique<double>(metadata->modification_time->ToJsTime());
    }

    if (names_.find(
            api::file_manager_private::ENTRY_PROPERTY_NAME_CONTENTMIMETYPE) !=
            names_.end() &&
        metadata->mime_type.get()) {
      properties_->content_mime_type =
          std::make_unique<std::string>(*metadata->mime_type);
    }

    if (names_.find(
            api::file_manager_private::ENTRY_PROPERTY_NAME_THUMBNAILURL) !=
            names_.end() &&
        metadata->thumbnail.get()) {
      properties_->thumbnail_url =
          std::make_unique<std::string>(*metadata->thumbnail);
    }

    CompleteGetEntryProperties(base::File::FILE_OK);
  }

  void CompleteGetEntryProperties(base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!callback_.is_null());

    std::move(callback_).Run(std::move(properties_), result);
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }

  // Given parameters.
  ResultCallback callback_;
  const storage::FileSystemURL file_system_url_;
  const std::set<EntryPropertyName> names_;

  // Values used in the process.
  std::unique_ptr<EntryProperties> properties_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForFileSystemProvider>
      weak_ptr_factory_{this};
};  // class SingleEntryPropertiesGetterForFileSystemProvider

class SingleEntryPropertiesGetterForDocumentsProvider {
 public:
  typedef base::OnceCallback<void(std::unique_ptr<EntryProperties> properties,
                                  base::File::Error error)>
      ResultCallback;

  // Creates an instance and starts the process.
  static void Start(const storage::FileSystemURL file_system_url,
                    Profile* const profile,
                    ResultCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    SingleEntryPropertiesGetterForDocumentsProvider* instance =
        new SingleEntryPropertiesGetterForDocumentsProvider(
            file_system_url, profile, std::move(callback));
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

 private:
  SingleEntryPropertiesGetterForDocumentsProvider(
      const storage::FileSystemURL& file_system_url,
      Profile* const profile,
      ResultCallback callback)
      : callback_(std::move(callback)),
        file_system_url_(file_system_url),
        profile_(profile),
        properties_(new EntryProperties) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!callback_.is_null());
  }

  void StartProcess() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    auto* root_map =
        arc::ArcDocumentsProviderRootMap::GetForBrowserContext(profile_);
    if (!root_map) {
      CompleteGetEntryProperties(base::File::FILE_ERROR_NOT_FOUND);
    }
    base::FilePath path;
    auto* root = root_map->ParseAndLookup(file_system_url_, &path);
    if (!root) {
      CompleteGetEntryProperties(base::File::FILE_ERROR_NOT_FOUND);
      return;
    }
    root->GetExtraFileMetadata(
        path, base::BindOnce(&SingleEntryPropertiesGetterForDocumentsProvider::
                                 OnGetExtraFileMetadata,
                             weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetExtraFileMetadata(
      base::File::Error error,
      const arc::ArcDocumentsProviderRoot::ExtraFileMetadata& metadata) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (error != base::File::FILE_OK) {
      CompleteGetEntryProperties(error);
      return;
    }
    properties_->can_delete = std::make_unique<bool>(metadata.supports_delete);
    properties_->can_rename = std::make_unique<bool>(metadata.supports_rename);
    properties_->can_add_children =
        std::make_unique<bool>(metadata.dir_supports_create);
    properties_->modification_time =
        std::make_unique<double>(metadata.last_modified.ToJsTimeIgnoringNull());
    properties_->size = std::make_unique<double>(metadata.size);
    CompleteGetEntryProperties(base::File::FILE_OK);
  }

  void CompleteGetEntryProperties(base::File::Error error) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(callback_);

    std::move(callback_).Run(std::move(properties_), error);
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }

  // Given parameters.
  ResultCallback callback_;
  const storage::FileSystemURL file_system_url_;
  Profile* const profile_;

  // Values used in the process.
  std::unique_ptr<EntryProperties> properties_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForDocumentsProvider>
      weak_ptr_factory_{this};
};  // class SingleEntryPropertiesGetterForDocumentsProvider

void OnSearchDriveFs(
    scoped_refptr<ExtensionFunction> function,
    bool filter_dirs,
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback,
    drive::FileError error,
    base::Optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  Profile* const profile =
      Profile::FromBrowserContext(function->browser_context());
  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!integration_service) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (error != drive::FILE_ERROR_OK || !items.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  GURL url;
  file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile, integration_service->GetMountPointPath(),
      function->extension_id(), &url);
  const auto fs_root = base::StrCat({url.spec(), "/"});
  const auto fs_name =
      integration_service->GetMountPointPath().BaseName().value();
  const base::FilePath root("/");

  auto result = std::make_unique<base::ListValue>();
  for (const auto& item : *items) {
    base::FilePath path;
    if (!root.AppendRelativePath(item->path, &path))
      path = item->path;
    base::DictionaryValue entry;
    entry.SetKey("fileSystemName", base::Value(fs_name));
    entry.SetKey("fileSystemRoot", base::Value(fs_root));
    entry.SetKey("fileFullPath", base::Value(item->path.AsUTF8Unsafe()));
    bool is_dir = drivefs::IsADirectory(item->metadata->type);
    entry.SetKey("fileIsDirectory", base::Value(is_dir));
    entry.SetKey(kAvailableOfflinePropertyName,
                 base::Value(item->metadata->available_offline));
    if (!filter_dirs || !is_dir) {
      result->Append(std::move(entry));
    }
  }

  std::move(callback).Run(std::move(result));
}

drivefs::mojom::QueryParameters::QuerySource SearchDriveFs(
    scoped_refptr<ExtensionFunction> function,
    drivefs::mojom::QueryParametersPtr query,
    bool filter_dirs,
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback) {
  drive::DriveIntegrationService* const integration_service =
      drive::util::GetIntegrationServiceByProfile(
          Profile::FromBrowserContext(function->browser_context()));
  auto on_response = base::BindOnce(&OnSearchDriveFs, std::move(function),
                                    filter_dirs, std::move(callback));
  return integration_service->GetDriveFsHost()->PerformSearch(
      std::move(query),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(on_response), drive::FileError::FILE_ERROR_ABORT,
          base::Optional<std::vector<drivefs::mojom::QueryItemPtr>>()));
}

void UmaEmitSearchOutcome(
    bool success,
    bool remote,
    FileManagerPrivateSearchDriveMetadataFunction::SearchType type,
    const base::TimeTicks& time_started) {
  const char* infix = nullptr;
  switch (type) {
    case FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText:
      infix = "TextSearchTime";
      break;
    case FileManagerPrivateSearchDriveMetadataFunction::SearchType::
        kSharedWithMe:
      infix = "SharedSearchTime";
      break;
    case FileManagerPrivateSearchDriveMetadataFunction::SearchType::kOffline:
      infix = "OfflineSearchTime";
      break;
  }
  if (remote) {
    base::UmaHistogramMediumTimes(
        base::StrCat({"DriveCommon.RemoteSearch.", infix,
                      success ? ".SuccessTime" : ".FailTime"}),
        base::TimeTicks::Now() - time_started);
  } else {
    base::UmaHistogramMediumTimes(
        base::StrCat({"DriveCommon.LocalSearch.", infix,
                      success ? ".SuccessTime" : ".FailTime"}),
        base::TimeTicks::Now() - time_started);
  }
}

}  // namespace

FileManagerPrivateInternalGetEntryPropertiesFunction::
    FileManagerPrivateInternalGetEntryPropertiesFunction()
    : processed_count_(0) {
  SetWarningThresholds(kDriveSlowOperationThreshold,
                       kDriveVerySlowOperationThreshold);
}

FileManagerPrivateInternalGetEntryPropertiesFunction::
    ~FileManagerPrivateInternalGetEntryPropertiesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetEntryPropertiesFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using api::file_manager_private_internal::GetEntryProperties::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  properties_list_.resize(params->urls.size());
  const std::set<EntryPropertyName> names_as_set(params->names.begin(),
                                                 params->names.end());
  for (size_t i = 0; i < params->urls.size(); i++) {
    const GURL url = GURL(params->urls[i]);
    const storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(url);
    switch (file_system_url.type()) {
      case storage::kFileSystemTypeProvided:
        SingleEntryPropertiesGetterForFileSystemProvider::Start(
            file_system_url, names_as_set,
            base::BindOnce(
                &FileManagerPrivateInternalGetEntryPropertiesFunction::
                    CompleteGetEntryProperties,
                this, i, file_system_url));
        break;
      case storage::kFileSystemTypeDriveFs:
        file_manager::util::SingleEntryPropertiesGetterForDriveFs::Start(
            file_system_url, profile,
            base::BindOnce(
                &FileManagerPrivateInternalGetEntryPropertiesFunction::
                    CompleteGetEntryProperties,
                this, i, file_system_url));
        break;
      case storage::kFileSystemTypeArcDocumentsProvider:
        SingleEntryPropertiesGetterForDocumentsProvider::Start(
            file_system_url, profile,
            base::BindOnce(
                &FileManagerPrivateInternalGetEntryPropertiesFunction::
                    CompleteGetEntryProperties,
                this, i, file_system_url));
        break;
      default:
        // TODO(yawano) Change this to support other voluems (e.g. local) ,and
        // integrate fileManagerPrivate.getMimeType to this method.
        LOG(ERROR) << "Not supported file system type.";
        CompleteGetEntryProperties(i, file_system_url,
                                   base::WrapUnique(new EntryProperties),
                                   base::File::FILE_ERROR_INVALID_OPERATION);
    }
  }

  return RespondLater();
}

void FileManagerPrivateInternalGetEntryPropertiesFunction::
    CompleteGetEntryProperties(size_t index,
                               const storage::FileSystemURL& url,
                               std::unique_ptr<EntryProperties> properties,
                               base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(0 <= processed_count_ && processed_count_ < properties_list_.size());

  if (error == base::File::FILE_OK) {
    properties->external_file_url = std::make_unique<std::string>(
        chromeos::FileSystemURLToExternalFileURL(url).spec());
  }
  properties_list_[index] = std::move(*properties);

  processed_count_++;
  if (processed_count_ < properties_list_.size())
    return;

  Respond(
      ArgumentList(extensions::api::file_manager_private_internal::
                       GetEntryProperties::Results::Create(properties_list_)));
}

FileManagerPrivateInternalPinDriveFileFunction::
    FileManagerPrivateInternalPinDriveFileFunction() {
  SetWarningThresholds(kDriveSlowOperationThreshold,
                       kDriveVerySlowOperationThreshold);
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalPinDriveFileFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::PinDriveFile::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  switch (file_system_url.type()) {
    case storage::kFileSystemTypeDriveFs:
      return RunAsyncForDriveFs(file_system_url, params->pin);

    default:
      return RespondNow(Error("Invalid file system type"));
  }
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalPinDriveFileFunction::RunAsyncForDriveFs(
    const storage::FileSystemURL& file_system_url,
    bool pin) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          Profile::FromBrowserContext(browser_context()));
  base::FilePath path;
  if (!integration_service || !integration_service->GetRelativeDrivePath(
                                  file_system_url.path(), &path)) {
    return RespondNow(Error("Drive is disabled"));
  }

  auto* drivefs_interface = integration_service->GetDriveFsInterface();
  if (!drivefs_interface)
    return RespondNow(Error("Drive is disabled"));

  drivefs_interface->SetPinned(
      path, pin,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &FileManagerPrivateInternalPinDriveFileFunction::OnPinStateSet,
              this),
          drive::FILE_ERROR_SERVICE_UNAVAILABLE));
  return RespondLater();
}

void FileManagerPrivateInternalPinDriveFileFunction::OnPinStateSet(
    drive::FileError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error == drive::FILE_ERROR_OK) {
    Respond(NoArguments());
  } else {
    Respond(Error(drive::FileErrorToString(error)));
  }
}

FileManagerPrivateSearchDriveFunction::FileManagerPrivateSearchDriveFunction() {
  SetWarningThresholds(kDriveSlowOperationThreshold,
                       kDriveVerySlowOperationThreshold);
}

ExtensionFunction::ResponseAction FileManagerPrivateSearchDriveFunction::Run() {
  using extensions::api::file_manager_private::SearchDrive::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!drive::util::GetIntegrationServiceByProfile(
          Profile::FromBrowserContext(browser_context()))) {
    // |integration_service| is NULL if Drive is disabled or not mounted.
    return RespondNow(Error("Drive is disabled"));
  }

  operation_start_ = base::TimeTicks::Now();
  is_offline_ = content::GetNetworkConnectionTracker()->IsOffline();

  auto query = drivefs::mojom::QueryParameters::New();
  query->text_content = params->search_params.query;
  is_offline_ =
      SearchDriveFs(
          this, std::move(query), false,
          base::BindOnce(
              &FileManagerPrivateSearchDriveFunction::OnSearchDriveFs, this)) ==
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;

  return RespondLater();
}

void FileManagerPrivateSearchDriveFunction::OnSearchDriveFs(
    std::unique_ptr<base::ListValue> results) {
  if (!results) {
    UmaEmitSearchOutcome(
        false, !is_offline_,
        FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
        operation_start_);
    Respond(Error("No search results"));
    return;
  }
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetKey("entries", std::move(*results));
  // Search queries are capped at 100 of items anyway and pagination is
  // never actually used, so no need to fill this.
  result->SetKey("nextFeed", base::Value(""));
  UmaEmitSearchOutcome(
      true, !is_offline_,
      FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
      operation_start_);
  Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(result))));
}

FileManagerPrivateSearchDriveMetadataFunction::
    FileManagerPrivateSearchDriveMetadataFunction() {
  SetWarningThresholds(kDriveSlowOperationThreshold,
                       kDriveVerySlowOperationThreshold);
}

ExtensionFunction::ResponseAction
FileManagerPrivateSearchDriveMetadataFunction::Run() {
  using api::file_manager_private::SearchDriveMetadata::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  drive::EventLogger* logger = file_manager::util::GetLogger(profile);
  if (logger) {
    logger->Log(
        logging::LOG_INFO, "%s[%d] called. (types: '%s', maxResults: '%d')",
        name(), request_id(),
        api::file_manager_private::ToString(params->search_params.types),
        params->search_params.max_results);
  }
  set_log_on_completion(true);

  drive::DriveIntegrationService* const integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!integration_service) {
    // |integration_service| is NULL if Drive is disabled or not mounted.
    return RespondNow(Error("Drive not available"));
  }

  operation_start_ = base::TimeTicks::Now();
  is_offline_ = true;  // Legacy search is assumed offline always.

  auto query = drivefs::mojom::QueryParameters::New();
  query->sort_field = drivefs::mojom::QueryParameters::SortField::kLastModified;
  query->sort_direction =
      drivefs::mojom::QueryParameters::SortDirection::kDescending;
  if (!params->search_params.query.empty()) {
    query->title = params->search_params.query;
    query->query_source =
        drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  }
  query->page_size = params->search_params.max_results;
  bool filter_dirs = false;
  switch (params->search_params.types) {
    case api::file_manager_private::SEARCH_TYPE_EXCLUDE_DIRECTORIES:
      filter_dirs = true;
      search_type_ = SearchType::kText;
      break;
    case api::file_manager_private::SEARCH_TYPE_SHARED_WITH_ME:
      query->shared_with_me = true;
      search_type_ = SearchType::kSharedWithMe;
      break;
    case api::file_manager_private::SEARCH_TYPE_OFFLINE:
      query->available_offline = true;
      query->query_source =
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
      search_type_ = SearchType::kOffline;
      break;
    case api::file_manager_private::SEARCH_TYPE_ALL:
      search_type_ = SearchType::kText;
      break;
    default:
      return RespondNow(Error("Invalid search type"));
  }
  is_offline_ =
      SearchDriveFs(
          this, std::move(query), filter_dirs,
          base::BindOnce(
              &FileManagerPrivateSearchDriveMetadataFunction::OnSearchDriveFs,
              this, params->search_params.query)) ==
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;

  return RespondLater();
}

void FileManagerPrivateSearchDriveMetadataFunction::OnSearchDriveFs(
    const std::string& query_text,
    std::unique_ptr<base::ListValue> results) {
  if (!results) {
    UmaEmitSearchOutcome(false, !is_offline_, search_type_, operation_start_);
    Respond(Error("No search results"));
    return;
  }

  std::vector<std::u16string> keywords =
      base::SplitString(base::UTF8ToUTF16(query_text),
                        base::StringPiece16(base::kWhitespaceUTF16),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::unique_ptr<
      base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>>
      queries;
  queries.reserve(keywords.size());
  for (const auto& keyword : keywords) {
    queries.push_back(
        std::make_unique<
            base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents>(
            keyword));
  }

  auto results_list = std::make_unique<base::ListValue>();
  for (auto& entry : *results) {
    base::DictionaryValue dict;
    std::string highlight;
    base::Value* value = entry.FindKey("fileFullPath");
    if (value && value->GetAsString(&highlight)) {
      base::FilePath path(highlight);
      if (!drive::internal::FindAndHighlight(path.BaseName().value(), queries,
                                             &highlight)) {
        highlight = path.BaseName().value();
      }
    }
    if (base::Value* availableOffline = entry.FindKeyOfType(
            kAvailableOfflinePropertyName, base::Value::Type::BOOLEAN)) {
      dict.SetKey(kAvailableOfflinePropertyName, std::move(*availableOffline));
      entry.RemoveKey(kAvailableOfflinePropertyName);
    }
    dict.SetKey("entry", std::move(entry));
    dict.SetKey("highlightedBaseName", base::Value(highlight));
    results_list->Append(std::move(dict));
  }

  UmaEmitSearchOutcome(true, !is_offline_, search_type_, operation_start_);
  Respond(
      OneArgument(base::Value::FromUniquePtrValue(std::move(results_list))));
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDriveConnectionStateFunction::Run() {
  api::file_manager_private::DriveConnectionState result;

  switch (drive::util::GetDriveConnectionStatus(
      Profile::FromBrowserContext(browser_context()))) {
    case drive::util::DRIVE_DISCONNECTED_NOSERVICE:
      result.type =
          api::file_manager_private::DRIVE_CONNECTION_STATE_TYPE_OFFLINE;
      result.reason =
          api::file_manager_private::DRIVE_OFFLINE_REASON_NO_SERVICE;
      break;
    case drive::util::DRIVE_DISCONNECTED_NONETWORK:
      result.type =
          api::file_manager_private::DRIVE_CONNECTION_STATE_TYPE_OFFLINE;
      result.reason =
          api::file_manager_private::DRIVE_OFFLINE_REASON_NO_NETWORK;
      break;
    case drive::util::DRIVE_DISCONNECTED_NOTREADY:
      result.type =
          api::file_manager_private::DRIVE_CONNECTION_STATE_TYPE_OFFLINE;
      result.reason = api::file_manager_private::DRIVE_OFFLINE_REASON_NOT_READY;
      break;
    case drive::util::DRIVE_CONNECTED_METERED:
      result.type =
          api::file_manager_private::DRIVE_CONNECTION_STATE_TYPE_METERED;
      break;
    case drive::util::DRIVE_CONNECTED:
      result.type =
          api::file_manager_private::DRIVE_CONNECTION_STATE_TYPE_ONLINE;
      break;
  }

  result.has_cellular_network_access =
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->FirstNetworkByType(chromeos::NetworkTypePattern::Mobile());

  const auto& enabled_extensions =
      extensions::ExtensionRegistry::Get(browser_context())
          ->enabled_extensions();
  result.can_pin_hosted_files =
      base::FeatureList::IsEnabled(
          chromeos::features::kDriveFsBidirectionalNativeMessaging) &&
      enabled_extensions.Contains(extension_misc::kDocsOfflineExtensionId) &&
      enabled_extensions.Contains(
          GURL(drive::kDriveFsNativeMessageHostOrigins[0]).host());

  return RespondNow(ArgumentList(
      api::file_manager_private::GetDriveConnectionState::Results::Create(
          result)));
}

FileManagerPrivateInternalGetDownloadUrlFunction::
    FileManagerPrivateInternalGetDownloadUrlFunction() {
  SetWarningThresholds(kDriveSlowOperationThreshold,
                       kDriveVerySlowOperationThreshold);
}

FileManagerPrivateInternalGetDownloadUrlFunction::
    ~FileManagerPrivateInternalGetDownloadUrlFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDownloadUrlFunction::Run() {
  using extensions::api::file_manager_private_internal::GetDownloadUrl::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  switch (file_system_url.type()) {
    case storage::kFileSystemTypeDriveFs:
      return RunAsyncForDriveFs(file_system_url);
    default:
      return RespondNow(Error("Drive is disabled"));
  }
}

void FileManagerPrivateInternalGetDownloadUrlFunction::OnGotDownloadUrl(
    GURL download_url) {
  if (download_url.is_empty()) {
    // Intentionally returns a blank.
    Respond(Error("Download Url for this item is not available."));
    return;
  }
  download_url_ = std::move(download_url);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  // This class doesn't care about browser sync consent.
  const CoreAccountId& account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  std::vector<std::string> scopes;
  scopes.emplace_back("https://www.googleapis.com/auth/drive.readonly");

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(browser_context())
          ->GetURLLoaderFactoryForBrowserProcess();
  auth_service_ = std::make_unique<google_apis::AuthService>(
      identity_manager, account_id, url_loader_factory, scopes);
  auth_service_->StartAuthentication(base::BindOnce(
      &FileManagerPrivateInternalGetDownloadUrlFunction::OnTokenFetched, this));
}

void FileManagerPrivateInternalGetDownloadUrlFunction::OnTokenFetched(
    google_apis::DriveApiErrorCode code,
    const std::string& access_token) {
  if (code != google_apis::HTTP_SUCCESS) {
    // Intentionally returns a blank.
    Respond(Error("Not able to fetch the token."));
    return;
  }

  Respond(OneArgument(base::Value(
      download_url_.Resolve("?alt=media&access_token=" + access_token)
          .spec())));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetDownloadUrlFunction::RunAsyncForDriveFs(
    const storage::FileSystemURL& file_system_url) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          Profile::FromBrowserContext(browser_context()));
  base::FilePath path;
  if (!integration_service || !integration_service->GetRelativeDrivePath(
                                  file_system_url.path(), &path)) {
    return RespondNow(Error("Drive not available"));
  }

  auto* drivefs_interface = integration_service->GetDriveFsInterface();
  if (!drivefs_interface)
    return RespondNow(Error("Drive not available"));

  drivefs_interface->GetMetadata(
      path,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &FileManagerPrivateInternalGetDownloadUrlFunction::OnGotMetadata,
              this),
          drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
  return RespondLater();
}

void FileManagerPrivateInternalGetDownloadUrlFunction::OnGotMetadata(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  OnGotDownloadUrl(metadata ? GURL(metadata->download_url) : GURL());
}

ExtensionFunction::ResponseAction
FileManagerPrivateNotifyDriveDialogResultFunction::Run() {
  using api::file_manager_private::NotifyDriveDialogResult::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  file_manager::EventRouter* const event_router =
      file_manager::EventRouterFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (event_router) {
    drivefs::mojom::DialogResult result;
    switch (params->result) {
      case api::file_manager_private::DRIVE_DIALOG_RESULT_NONE:
      case api::file_manager_private::DRIVE_DIALOG_RESULT_NOT_DISPLAYED:
        result = drivefs::mojom::DialogResult::kNotDisplayed;
        break;
      case api::file_manager_private::DRIVE_DIALOG_RESULT_ACCEPT:
        result = drivefs::mojom::DialogResult::kAccept;
        break;
      case api::file_manager_private::DRIVE_DIALOG_RESULT_REJECT:
        result = drivefs::mojom::DialogResult::kReject;
        break;
      case api::file_manager_private::DRIVE_DIALOG_RESULT_DISMISS:
        result = drivefs::mojom::DialogResult::kDismiss;
        break;
    }
    event_router->OnDriveDialogResult(result);
  } else {
    return RespondNow(Error("Could not find event router"));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
