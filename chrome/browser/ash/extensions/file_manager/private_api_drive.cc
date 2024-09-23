// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_drive.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/string_search.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/fileapi/recent_drive_source.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host.h"
#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host_origins.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_dialog.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/drive/chromeos/search_metadata.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "google_apis/gaia/gaia_constants.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/gurl.h"

namespace extensions {
namespace {

using ash::file_system_provider::EntryMetadata;
using ash::file_system_provider::ProvidedFileSystemInterface;
using ash::file_system_provider::util::FileSystemURLParser;
using content::BrowserThread;
using drive::DriveIntegrationService;
using drive::util::GetIntegrationServiceByProfile;
using drivefs::pinning::PinningManager;
using extensions::api::file_manager_private::EntryProperties;
using extensions::api::file_manager_private::EntryPropertyName;
using file_manager::util::EntryDefinition;
using file_manager::util::EntryDefinitionCallback;
using file_manager::util::EntryDefinitionList;
using file_manager::util::EntryDefinitionListCallback;
using file_manager::util::FileDefinition;
using file_manager::util::FileDefinitionList;
using google_apis::DriveApiUrlGenerator;

constexpr char kAvailableOfflinePropertyName[] = "availableOffline";

// Thresholds for logging slow operations.
constexpr base::TimeDelta kDriveSlowOperationThreshold = base::Seconds(5);
constexpr base::TimeDelta kDriveVerySlowOperationThreshold = base::Minutes(1);

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
    if (names_.find(api::file_manager_private::EntryPropertyName::kSize) !=
        names_.end()) {
      field_mask |= ProvidedFileSystemInterface::METADATA_FIELD_SIZE;
    }
    if (names_.find(
            api::file_manager_private::EntryPropertyName::kModificationTime) !=
        names_.end()) {
      field_mask |=
          ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME;
    }
    if (names_.find(
            api::file_manager_private::EntryPropertyName::kContentMimeType) !=
        names_.end()) {
      field_mask |= ProvidedFileSystemInterface::METADATA_FIELD_MIME_TYPE;
    }
    if (names_.find(
            api::file_manager_private::EntryPropertyName::kThumbnailUrl) !=
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

    if (names_.find(api::file_manager_private::EntryPropertyName::kSize) !=
        names_.end()) {
      properties_->size = *metadata->size;
    }

    if (names_.find(
            api::file_manager_private::EntryPropertyName::kModificationTime) !=
        names_.end()) {
      properties_->modification_time =
          metadata->modification_time->InMillisecondsFSinceUnixEpoch();
    }

    if (names_.find(
            api::file_manager_private::EntryPropertyName::kContentMimeType) !=
            names_.end() &&
        metadata->mime_type.get()) {
      properties_->content_mime_type = *metadata->mime_type;
    }

    if (names_.find(
            api::file_manager_private::EntryPropertyName::kThumbnailUrl) !=
            names_.end() &&
        metadata->thumbnail.get()) {
      properties_->thumbnail_url = *metadata->thumbnail;
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
        file_system_url_(ResolveFuseBoxFSURL(profile, file_system_url)),
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

  static storage::FileSystemURL ResolveFuseBoxFSURL(
      Profile* profile,
      const storage::FileSystemURL& file_system_url) {
    if (!base::StartsWith(file_system_url.filesystem_id(),
                          file_manager::util::kFuseBoxMountNamePrefix)) {
      return file_system_url;
    }
    fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
    if (!fusebox_server) {
      return storage::FileSystemURL();
    }
    return fusebox_server->ResolveFilename(profile,
                                           file_system_url.path().value());
  }

  void OnGetExtraFileMetadata(
      base::File::Error error,
      const arc::ArcDocumentsProviderRoot::ExtraFileMetadata& metadata) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (error != base::File::FILE_OK) {
      CompleteGetEntryProperties(error);
      return;
    }
    properties_->can_delete = metadata.supports_delete;
    properties_->can_rename = metadata.supports_rename;
    properties_->can_add_children = metadata.dir_supports_create;
    if (!metadata.last_modified.is_null()) {
      properties_->modification_time =
          metadata.last_modified.InMillisecondsFSinceUnixEpochIgnoringNull();
    }
    properties_->size = metadata.size;
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
  const raw_ptr<Profile> profile_;

  // Values used in the process.
  std::unique_ptr<EntryProperties> properties_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForDocumentsProvider>
      weak_ptr_factory_{this};
};  // class SingleEntryPropertiesGetterForDocumentsProvider

void OnSearchDriveFs(
    scoped_refptr<ExtensionFunction> function,
    bool filter_dirs,
    base::OnceCallback<void(std::optional<base::Value::List>)> callback,
    drive::FileError error,
    std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  Profile* const profile =
      Profile::FromBrowserContext(function->browser_context());
  DriveIntegrationService* const service =
      GetIntegrationServiceByProfile(profile);
  if (!service) {
    LOG(ERROR) << "No drive service";
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (error != drive::FILE_ERROR_OK || !items.has_value()) {
    LOG_IF(ERROR, error != drive::FILE_ERROR_OK)
        << "Drive search failed: " << drive::FileErrorToString(error);
    std::move(callback).Run(std::nullopt);
    return;
  }

  GURL url;
  file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
      profile, service->GetMountPointPath(), function->source_url(), &url);
  const auto fs_root = base::StrCat({url.spec(), "/"});
  const auto fs_name = service->GetMountPointPath().BaseName().value();
  const base::FilePath root("/");

  base::Value::List result;
  for (const auto& item : *items) {
    base::FilePath path;
    if (!root.AppendRelativePath(item->path, &path)) {
      path = item->path;
    }
    base::Value::Dict entry;
    entry.Set("fileSystemName", fs_name);
    entry.Set("fileSystemRoot", fs_root);
    entry.Set("fileFullPath", item->path.AsUTF8Unsafe());
    bool is_dir = drivefs::IsADirectory(item->metadata->type);
    entry.Set("fileIsDirectory", is_dir);
    entry.Set(kAvailableOfflinePropertyName, item->metadata->available_offline);
    if (!filter_dirs || !is_dir) {
      result.Append(std::move(entry));
    }
  }

  std::move(callback).Run(std::move(result));
}

drivefs::mojom::QueryParameters::QuerySource SearchDriveFs(
    scoped_refptr<ExtensionFunction> function,
    drivefs::mojom::QueryParametersPtr query,
    bool filter_dirs,
    base::OnceCallback<void(std::optional<base::Value::List>)> callback) {
  DriveIntegrationService* const service = GetIntegrationServiceByProfile(
      Profile::FromBrowserContext(function->browser_context()));
  auto on_response = base::BindOnce(&OnSearchDriveFs, std::move(function),
                                    filter_dirs, std::move(callback));
  return service->GetDriveFsHost()->PerformSearch(std::move(query),
                                                  std::move(on_response));
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
  const std::optional<Params> params = Params::Create(args());
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
        file_system_context->CrackURLInFirstPartyContext(url);

    storage::FileSystemType file_system_type = file_system_url.type();
    if (file_system_type == storage::kFileSystemTypeFuseBox) {
      std::string_view path(file_system_url.path().value());
      if (base::StartsWith(path, file_manager::util::kFuseBoxMediaSlashPath)) {
        path.remove_prefix(strlen(file_manager::util::kFuseBoxMediaSlashPath));
        if (base::StartsWith(path,
                             file_manager::util::kFuseBoxSubdirPrefixADP)) {
          file_system_type = storage::kFileSystemTypeArcDocumentsProvider;
        } else if (base::StartsWith(
                       path, file_manager::util::kFuseBoxSubdirPrefixFSP)) {
          file_system_type = storage::kFileSystemTypeProvided;
        }
      }
    }

    auto callback =
        base::BindOnce(&FileManagerPrivateInternalGetEntryPropertiesFunction::
                           CompleteGetEntryProperties,
                       this, i, file_system_url);

    switch (file_system_type) {
      case storage::kFileSystemTypeProvided:
        SingleEntryPropertiesGetterForFileSystemProvider::Start(
            file_system_url, names_as_set, std::move(callback));
        break;
      case storage::kFileSystemTypeDriveFs:
        file_manager::util::SingleEntryPropertiesGetterForDriveFs::Start(
            file_system_url, profile, std::move(callback));
        break;
      case storage::kFileSystemTypeArcDocumentsProvider:
        SingleEntryPropertiesGetterForDocumentsProvider::Start(
            file_system_url, profile, std::move(callback));
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
    properties->external_file_url =
        ash::FileSystemURLToExternalFileURL(url).spec();
  }
  properties_list_[index] = std::move(*properties);

  processed_count_++;
  if (processed_count_ < properties_list_.size()) {
    return;
  }

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
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURLInFirstPartyContext(url);

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
  DriveIntegrationService* const service =
      drive::DriveIntegrationServiceFactory::FindForProfile(
          Profile::FromBrowserContext(browser_context()));
  base::FilePath path;
  if (!service ||
      !service->GetRelativeDrivePath(file_system_url.path(), &path)) {
    return RespondNow(Error("Drive is disabled"));
  }

  drivefs::mojom::DriveFs* const drivefs = service->GetDriveFsInterface();
  if (!drivefs) {
    return RespondNow(Error("Drive is disabled"));
  }

  drivefs->SetPinned(
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
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!GetIntegrationServiceByProfile(
          Profile::FromBrowserContext(browser_context()))) {
    // |integration_service| is NULL if Drive is disabled or not mounted.
    return RespondNow(Error("Drive is disabled"));
  }

  operation_start_ = base::TimeTicks::Now();
  is_offline_ = content::GetNetworkConnectionTracker()->IsOffline();

  auto query = drivefs::mojom::QueryParameters::New();
  query->text_content = params->search_params.query;
  if (params->search_params.modified_timestamp.has_value()) {
    query->modified_time = base::Time::FromMillisecondsSinceUnixEpoch(
        *params->search_params.modified_timestamp);
    query->modified_time_operator =
        drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  }
  ash::RecentSource::FileType file_type;
  if (!file_manager::util::ToRecentSourceFileType(
          params->search_params.category, &file_type)) {
    return RespondNow(Error("Unable to convert file category"));
  }
  auto type_filters = ash::RecentDriveSource::CreateTypeFilters(file_type);
  if (type_filters.size() == 1) {
    query->mime_type = type_filters.front();
  } else if (type_filters.size() > 1) {
    query->mime_types = std::move(type_filters);
  }
  is_offline_ =
      SearchDriveFs(
          this, std::move(query), false,
          base::BindOnce(
              &FileManagerPrivateSearchDriveFunction::OnSearchDriveFs, this)) ==
      drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;

  return RespondLater();
}

void FileManagerPrivateSearchDriveFunction::OnSearchDriveFs(
    std::optional<base::Value::List> results) {
  using api::file_manager_private::SearchDriveResponse;
  if (!results) {
    UmaEmitSearchOutcome(
        false, !is_offline_,
        FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
        operation_start_);
    Respond(Error("No search results"));
    return;
  }

  SearchDriveResponse response;
  // Search queries are capped at 100 of items anyway and pagination is
  // never actually used, so no need to fill this.
  response.next_feed = "";
  response.entries.reserve(results.value().size());
  for (const auto& e : results.value()) {
    auto entry = SearchDriveResponse::EntriesType::FromValue(e);
    if (!entry) {
      LOG(ERROR) << "Failed to convert entry: " << e.DebugString();
      continue;
    }
    response.entries.push_back(std::move(entry.value()));
  }

  UmaEmitSearchOutcome(
      true, !is_offline_,
      FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
      operation_start_);
  Respond(WithArguments(response.ToValue()));
}

FileManagerPrivateSearchDriveMetadataFunction::
    FileManagerPrivateSearchDriveMetadataFunction() {
  SetWarningThresholds(kDriveSlowOperationThreshold,
                       kDriveVerySlowOperationThreshold);
}

ExtensionFunction::ResponseAction
FileManagerPrivateSearchDriveMetadataFunction::Run() {
  using api::file_manager_private::SearchDriveMetadata::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  if (drive::EventLogger* logger = file_manager::util::GetLogger(profile)) {
    logger->Log(
        logging::LOGGING_INFO, "%s[%s] called. (types: '%s', maxResults: '%d')",
        name(), request_uuid().AsLowercaseString().c_str(),
        api::file_manager_private::ToString(params->search_params.types),
        params->search_params.max_results);
  }
  set_log_on_completion(true);

  if (!GetIntegrationServiceByProfile(profile)) {
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
  if (params->search_params.modified_timestamp.has_value()) {
    query->modified_time = base::Time::FromMillisecondsSinceUnixEpoch(
        *params->search_params.modified_timestamp);
    query->modified_time_operator =
        drivefs::mojom::QueryParameters::DateComparisonOperator::kGreaterThan;
  }
  ash::RecentSource::FileType file_type;
  if (!file_manager::util::ToRecentSourceFileType(
          params->search_params.category, &file_type)) {
    return RespondNow(Error("Unable to convert file category"));
  }
  auto type_filters = ash::RecentDriveSource::CreateTypeFilters(file_type);
  if (type_filters.size() == 1) {
    query->mime_type = type_filters.front();
  } else if (type_filters.size() > 1) {
    query->mime_types = std::move(type_filters);
  }
  query->page_size = params->search_params.max_results;
  bool filter_dirs = false;
  switch (params->search_params.types) {
    case api::file_manager_private::SearchType::kExcludeDirectories:
      filter_dirs = true;
      search_type_ = SearchType::kText;
      break;
    case api::file_manager_private::SearchType::kSharedWithMe:
      query->shared_with_me = true;
      search_type_ = SearchType::kSharedWithMe;
      break;
    case api::file_manager_private::SearchType::kOffline:
      query->available_offline = true;
      query->query_source =
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
      search_type_ = SearchType::kOffline;
      break;
    case api::file_manager_private::SearchType::kAll:
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
    std::optional<base::Value::List> results) {
  if (!results) {
    UmaEmitSearchOutcome(false, !is_offline_, search_type_, operation_start_);
    Respond(Error("No search results"));
    return;
  }

  std::vector<std::u16string> keywords =
      base::SplitString(base::UTF8ToUTF16(query_text),
                        std::u16string_view(base::kWhitespaceUTF16),
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

  base::Value::List results_list;
  for (auto& item : *results) {
    base::Value::Dict& entry = item.GetDict();

    base::Value::Dict dict;
    std::string highlight;
    std::string* value = entry.FindString("fileFullPath");
    if (value) {
      highlight = *value;
      base::FilePath path(highlight);
      if (!drive::internal::FindAndHighlight(path.BaseName().value(), queries,
                                             &highlight)) {
        highlight = path.BaseName().value();
      }
    }
    if (auto availableOffline = entry.FindBool(kAvailableOfflinePropertyName)) {
      dict.Set(kAvailableOfflinePropertyName, *availableOffline);
      entry.Remove(kAvailableOfflinePropertyName);
    }
    dict.Set("entry", std::move(entry));
    dict.Set("highlightedBaseName", highlight);
    results_list.Append(std::move(dict));
  }

  UmaEmitSearchOutcome(true, !is_offline_, search_type_, operation_start_);
  Respond(WithArguments(std::move(results_list)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDriveConnectionStateFunction::Run() {
  api::file_manager_private::DriveConnectionState result;

  using enum drive::util::ConnectionStatus;
  switch (drive::util::GetDriveConnectionStatus(
      Profile::FromBrowserContext(browser_context()))) {
    case kNoService:
      result.type =
          api::file_manager_private::DriveConnectionStateType::kOffline;
      result.reason = api::file_manager_private::DriveOfflineReason::kNoService;
      break;
    case kNoNetwork:
      result.type =
          api::file_manager_private::DriveConnectionStateType::kOffline;
      result.reason = api::file_manager_private::DriveOfflineReason::kNoNetwork;
      break;
    case kNotReady:
      result.type =
          api::file_manager_private::DriveConnectionStateType::kOffline;
      result.reason = api::file_manager_private::DriveOfflineReason::kNotReady;
      break;
    case kMetered:
      result.type =
          api::file_manager_private::DriveConnectionStateType::kMetered;
      break;
    case kConnected:
      result.type =
          api::file_manager_private::DriveConnectionStateType::kOnline;
      break;
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetDriveConnectionState::Results::Create(
          result)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateNotifyDriveDialogResultFunction::Run() {
  using api::file_manager_private::NotifyDriveDialogResult::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  file_manager::EventRouter* const event_router =
      file_manager::EventRouterFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (event_router) {
    drivefs::mojom::DialogResult result;
    switch (params->result) {
      case api::file_manager_private::DriveDialogResult::kNone:
      case api::file_manager_private::DriveDialogResult::kNotDisplayed:
        result = drivefs::mojom::DialogResult::kNotDisplayed;
        break;
      case api::file_manager_private::DriveDialogResult::kAccept:
        result = drivefs::mojom::DialogResult::kAccept;
        break;
      case api::file_manager_private::DriveDialogResult::kReject:
        result = drivefs::mojom::DialogResult::kReject;
        break;
      case api::file_manager_private::DriveDialogResult::kDismiss:
        result = drivefs::mojom::DialogResult::kDismiss;
        break;
    }
    event_router->OnDriveDialogResult(result);
  } else {
    return RespondNow(Error("Could not find event router"));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivatePollDriveHostedFilePinStatesFunction::Run() {
  if (DriveIntegrationService* const service = GetIntegrationServiceByProfile(
          Profile::FromBrowserContext(browser_context()))) {
    service->PollHostedFilePinStates();
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetBulkPinProgressFunction::Run() {
  DriveIntegrationService* const service = GetIntegrationServiceByProfile(
      Profile::FromBrowserContext(browser_context()));
  if (!service) {
    return RespondNow(Error("Drive not available"));
  }

  PinningManager* const p = service->GetPinningManager();
  if (!p) {
    return RespondNow(Error("Pin Manager not available"));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetBulkPinProgress::Results::Create(
          file_manager::util::BulkPinProgressToJs(p->GetProgress()))));
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenManageSyncSettingsFunction::Run() {
  if (ash::features::IsDriveFsMirroringEnabled()) {
    ash::ManageMirrorSyncDialog::Show(
        Profile::FromBrowserContext(browser_context()));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateCalculateBulkPinRequiredSpaceFunction::Run() {
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  DriveIntegrationService* const service =
      GetIntegrationServiceByProfile(profile);
  if (!service) {
    return RespondNow(Error("Drive not available"));
  }

  PinningManager* const p = service->GetPinningManager();
  if (!p) {
    return RespondNow(Error("Pin Manager not available"));
  }

  if (!p->CalculateRequiredSpace()) {
    return RespondNow(Error("Pin Manager is already pinning"));
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
