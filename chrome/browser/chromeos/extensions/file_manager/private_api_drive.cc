// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/i18n/string_search.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/drive/chromeos/search_metadata.h"
#include "components/drive/event_logger.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/drive_api_url_generator.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/network_change_notifier.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/common/fileapi/file_system_info.h"
#include "storage/common/fileapi/file_system_util.h"
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

// List of connection types of drive.
// Keep this in sync with the DriveConnectionType in common/js/util.js.
const char kDriveConnectionTypeOffline[] = "offline";
const char kDriveConnectionTypeMetered[] = "metered";
const char kDriveConnectionTypeOnline[] = "online";

// List of reasons of kDriveConnectionType*.
// Keep this in sync with the DriveConnectionReason in common/js/util.js.
const char kDriveConnectionReasonNotReady[] = "not_ready";
const char kDriveConnectionReasonNoNetwork[] = "no_network";
const char kDriveConnectionReasonNoService[] = "no_service";

// Maximum dimension of thumbnail in file manager. File manager shows 180x180
// thumbnail. Given that we support hdpi devices, maximum dimension is 360.
const int kFileManagerMaximumThumbnailDimension = 360;

std::unique_ptr<std::string> GetShareUrlFromAlternateUrl(
    const GURL& alternate_url) {
  // Set |share_url| to a modified version of |alternate_url| that opens the
  // sharing dialog for files and folders (add ?userstoinvite="" to the URL).
  // TODO(sashab): Add an endpoint to the Drive API that generates this URL,
  // instead of manually modifying it here.
  GURL::Replacements replacements;
  std::string new_query =
      (alternate_url.has_query() ? alternate_url.query() + "&" : "") +
      "userstoinvite=%22%22";
  replacements.SetQueryStr(new_query);

  return std::make_unique<std::string>(
      alternate_url.ReplaceComponents(replacements).spec());
}

// Copies properties from |entry_proto| to |properties|. |shared_with_me| is
// given from the running profile.
void FillEntryPropertiesValueForDrive(const drive::ResourceEntry& entry_proto,
                                      bool shared_with_me,
                                      EntryProperties* properties) {
  properties->shared_with_me = std::make_unique<bool>(shared_with_me);
  properties->shared = std::make_unique<bool>(entry_proto.shared());
  properties->starred = std::make_unique<bool>(entry_proto.starred());

  const drive::PlatformFileInfoProto& file_info = entry_proto.file_info();
  properties->size = std::make_unique<double>(file_info.size());
  properties->modification_time = std::make_unique<double>(
      base::Time::FromInternalValue(file_info.last_modified()).ToJsTime());
  properties->modification_by_me_time = std::make_unique<double>(
      base::Time::FromInternalValue(entry_proto.last_modified_by_me())
          .ToJsTime());

  if (entry_proto.has_alternate_url()) {
    properties->alternate_url =
        std::make_unique<std::string>(entry_proto.alternate_url());
    properties->share_url =
        GetShareUrlFromAlternateUrl(GURL(entry_proto.alternate_url()));
  }

  if (entry_proto.has_file_specific_info()) {
    const drive::FileSpecificInfo& file_specific_info =
        entry_proto.file_specific_info();

    if (!entry_proto.resource_id().empty()) {
      DriveApiUrlGenerator url_generator(
          (GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction)),
          (GURL(google_apis::DriveApiUrlGenerator::
                    kBaseThumbnailUrlForProduction)));
      properties->thumbnail_url = std::make_unique<std::string>(
          url_generator
              .GetThumbnailUrl(entry_proto.resource_id(), 500 /* width */,
                               500 /* height */, false /* not cropped */)
              .spec());
      properties->cropped_thumbnail_url = std::make_unique<std::string>(
          url_generator
              .GetThumbnailUrl(
                  entry_proto.resource_id(),
                  kFileManagerMaximumThumbnailDimension /* width */,
                  kFileManagerMaximumThumbnailDimension /* height */,
                  true /* cropped */)
              .spec());
    }
    if (file_specific_info.has_image_width()) {
      properties->image_width =
          std::make_unique<int>(file_specific_info.image_width());
    }
    if (file_specific_info.has_image_height()) {
      properties->image_height =
          std::make_unique<int>(file_specific_info.image_height());
    }
    if (file_specific_info.has_image_rotation()) {
      properties->image_rotation =
          std::make_unique<int>(file_specific_info.image_rotation());
    }
    properties->hosted =
        std::make_unique<bool>(file_specific_info.is_hosted_document());
    properties->content_mime_type =
        std::make_unique<std::string>(file_specific_info.content_mime_type());
    properties->pinned =
        std::make_unique<bool>(file_specific_info.cache_state().is_pinned());
    properties->dirty =
        std::make_unique<bool>(file_specific_info.cache_state().is_dirty());
    properties->present =
        std::make_unique<bool>(file_specific_info.cache_state().is_present());

    if (file_specific_info.cache_state().is_present()) {
      properties->available_offline = std::make_unique<bool>(true);
    } else if (file_specific_info.is_hosted_document() &&
               file_specific_info.has_document_extension()) {
      const std::string file_extension =
          file_specific_info.document_extension();
      // What's available offline? See the 'Web' column at:
      // https://support.google.com/drive/answer/1628467
      properties->available_offline = std::make_unique<bool>(
          file_extension == ".gdoc" || file_extension == ".gdraw" ||
          file_extension == ".gsheet" || file_extension == ".gslides");
    } else {
      properties->available_offline = std::make_unique<bool>(false);
    }

    properties->available_when_metered =
        std::make_unique<bool>(file_specific_info.cache_state().is_present() ||
                               file_specific_info.is_hosted_document());
  }

  if (entry_proto.has_capabilities_info()) {
    const drive::CapabilitiesInfo& capabilities_info =
        entry_proto.capabilities_info();

    // Only set the |can_copy| capability for hosted documents; for other files,
    // we must have read access, so |can_copy| is implicitly true.
    bool can_copy = true;
    if (entry_proto.has_file_specific_info() &&
        entry_proto.file_specific_info().is_hosted_document() &&
        capabilities_info.has_can_copy()) {
      can_copy = capabilities_info.can_copy();
    }
    properties->can_copy = std::make_unique<bool>(can_copy);

    properties->can_delete = std::make_unique<bool>(
        capabilities_info.has_can_delete() ? capabilities_info.can_delete()
                                           : true);
    properties->can_rename = std::make_unique<bool>(
        capabilities_info.has_can_rename() ? capabilities_info.can_rename()
                                           : true);

    // |can_add_children| defaults to true for directories, and false for files.
    properties->can_add_children =
        std::make_unique<bool>(capabilities_info.has_can_add_children()
                                   ? capabilities_info.can_add_children()
                                   : file_info.is_directory());

    properties->can_share = std::make_unique<bool>(
        capabilities_info.has_can_share() ? capabilities_info.can_share()
                                          : true);
  }
}

// Creates entry definition list for (metadata) search result info list.
template <class T>
void ConvertSearchResultInfoListToEntryDefinitionList(
    Profile* profile,
    const std::string& extension_id,
    const std::vector<T>& search_result_info_list,
    EntryDefinitionListCallback callback) {
  FileDefinitionList file_definition_list;

  for (size_t i = 0; i < search_result_info_list.size(); ++i) {
    FileDefinition file_definition;
    file_definition.virtual_path =
        file_manager::util::ConvertDrivePathToRelativeFileSystemPath(
            profile, extension_id, search_result_info_list.at(i).path);
    file_definition.is_directory = search_result_info_list.at(i).is_directory;
    file_definition_list.push_back(file_definition);
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      profile, extension_id,
      file_definition_list,  // Safe, since copied internally.
      std::move(callback));
}

class SingleEntryPropertiesGetterForDrive {
 public:
  typedef base::Callback<void(std::unique_ptr<EntryProperties> properties,
                              base::File::Error error)>
      ResultCallback;

  // Creates an instance and starts the process.
  static void Start(const base::FilePath local_path,
                    const std::set<EntryPropertyName>& names,
                    Profile* const profile,
                    const ResultCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    SingleEntryPropertiesGetterForDrive* instance =
        new SingleEntryPropertiesGetterForDrive(local_path, names, profile,
                                                callback);
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

  virtual ~SingleEntryPropertiesGetterForDrive() = default;

 private:
  SingleEntryPropertiesGetterForDrive(
      const base::FilePath local_path,
      const std::set<EntryPropertyName>& /* names */,
      Profile* const profile,
      const ResultCallback& callback)
      : callback_(callback),
        local_path_(local_path),
        running_profile_(profile),
        properties_(new EntryProperties),
        file_owner_profile_(nullptr),
        weak_ptr_factory_(this) {
    DCHECK(!callback_.is_null());
    DCHECK(profile);
  }

  void StartProcess() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    file_path_ = drive::util::ExtractDrivePath(local_path_);
    file_owner_profile_ = drive::util::ExtractProfileFromPath(local_path_);

    if (!file_owner_profile_ ||
        !g_browser_process->profile_manager()->IsValidProfile(
            file_owner_profile_)) {
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    // Start getting the file info.
    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(file_owner_profile_);
    if (!file_system) {
      // |file_system| is NULL if Drive is disabled or not mounted.
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    file_system->GetResourceEntry(
        file_path_,
        base::BindOnce(&SingleEntryPropertiesGetterForDrive::OnGetFileInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetFileInfo(drive::FileError error,
                     std::unique_ptr<drive::ResourceEntry> entry) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (error != drive::FILE_ERROR_OK) {
      CompleteGetEntryProperties(error);
      return;
    }

    DCHECK(entry);
    owner_resource_entry_.swap(entry);

    if (running_profile_->IsSameProfile(file_owner_profile_)) {
      StartParseFileInfo(owner_resource_entry_->shared_with_me());
      return;
    }

    // If the running profile does not own the file, obtain the shared_with_me
    // flag from the running profile's value.
    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(running_profile_);
    if (!file_system) {
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }
    file_system->GetPathFromResourceId(
        owner_resource_entry_->resource_id(),
        base::Bind(&SingleEntryPropertiesGetterForDrive::OnGetRunningPath,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetRunningPath(drive::FileError error,
                        const base::FilePath& file_path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (error != drive::FILE_ERROR_OK) {
      // The running profile does not know the file.
      StartParseFileInfo(false);
      return;
    }

    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(running_profile_);
    if (!file_system) {
      // The drive is disable for the running profile.
      StartParseFileInfo(false);
      return;
    }

    file_system->GetResourceEntry(
        file_path,
        base::BindOnce(&SingleEntryPropertiesGetterForDrive::OnGetShareInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGetShareInfo(drive::FileError error,
                      std::unique_ptr<drive::ResourceEntry> entry) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (error != drive::FILE_ERROR_OK) {
      CompleteGetEntryProperties(error);
      return;
    }

    DCHECK(entry.get());
    StartParseFileInfo(entry->shared_with_me());
  }

  void StartParseFileInfo(bool shared_with_me) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    FillEntryPropertiesValueForDrive(
        *owner_resource_entry_, shared_with_me, properties_.get());

    drive::FileSystemInterface* const file_system =
        drive::util::GetFileSystemByProfile(file_owner_profile_);
    if (!file_system) {
      // |file_system| is NULL if Drive is disabled.
      CompleteGetEntryProperties(drive::FILE_ERROR_FAILED);
      return;
    }

    // The properties meaningful for directories are already filled in
    // FillEntryPropertiesValueForDrive().
    if (!owner_resource_entry_->has_file_specific_info()) {
      CompleteGetEntryProperties(drive::FILE_ERROR_OK);
      return;
    }

    CompleteGetEntryProperties(drive::FILE_ERROR_OK);
  }

  void CompleteGetEntryProperties(drive::FileError error) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!callback_.is_null());

    callback_.Run(std::move(properties_),
                  drive::FileErrorToBaseFileError(error));
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }

  // Given parameters.
  const ResultCallback callback_;
  const base::FilePath local_path_;
  Profile* const running_profile_;

  // Values used in the process.
  std::unique_ptr<EntryProperties> properties_;
  Profile* file_owner_profile_;
  base::FilePath file_path_;
  std::unique_ptr<drive::ResourceEntry> owner_resource_entry_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForDrive> weak_ptr_factory_;
};  // class SingleEntryPropertiesGetterForDrive

class SingleEntryPropertiesGetterForFileSystemProvider {
 public:
  typedef base::Callback<void(std::unique_ptr<EntryProperties> properties,
                              base::File::Error error)>
      ResultCallback;

  // Creates an instance and starts the process.
  static void Start(const storage::FileSystemURL file_system_url,
                    const std::set<EntryPropertyName>& names,
                    const ResultCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    SingleEntryPropertiesGetterForFileSystemProvider* instance =
        new SingleEntryPropertiesGetterForFileSystemProvider(file_system_url,
                                                             names, callback);
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

  virtual ~SingleEntryPropertiesGetterForFileSystemProvider() = default;

 private:
  SingleEntryPropertiesGetterForFileSystemProvider(
      const storage::FileSystemURL& file_system_url,
      const std::set<EntryPropertyName>& names,
      const ResultCallback& callback)
      : callback_(callback),
        file_system_url_(file_system_url),
        names_(names),
        properties_(new EntryProperties),
        weak_ptr_factory_(this) {
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

    callback_.Run(std::move(properties_), result);
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }

  // Given parameters.
  const ResultCallback callback_;
  const storage::FileSystemURL file_system_url_;
  const std::set<EntryPropertyName> names_;

  // Values used in the process.
  std::unique_ptr<EntryProperties> properties_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForFileSystemProvider>
      weak_ptr_factory_;
};  // class SingleEntryPropertiesGetterForDrive

class SingleEntryPropertiesGetterForDriveFs {
 public:
  using ResultCallback =
      base::OnceCallback<void(std::unique_ptr<EntryProperties> properties,
                              base::File::Error error)>;

  // Creates an instance and starts the process.
  static void Start(const storage::FileSystemURL& file_system_url,
                    Profile* const profile,
                    ResultCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    SingleEntryPropertiesGetterForDriveFs* instance =
        new SingleEntryPropertiesGetterForDriveFs(file_system_url, profile,
                                                  std::move(callback));
    instance->StartProcess();

    // The instance will be destroyed by itself.
  }

 private:
  SingleEntryPropertiesGetterForDriveFs(
      const storage::FileSystemURL& file_system_url,
      Profile* const profile,
      ResultCallback callback)
      : callback_(std::move(callback)),
        file_system_url_(file_system_url),
        running_profile_(profile),
        properties_(std::make_unique<EntryProperties>()),
        weak_ptr_factory_(this) {
    DCHECK(callback_);
    DCHECK(profile);
  }

  void StartProcess() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(running_profile_);
    if (!integration_service || !integration_service->IsMounted()) {
      CompleteGetEntryProperties(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
      return;
    }
    base::FilePath path;
    if (!integration_service->GetRelativeDrivePath(file_system_url_.path(),
                                                   &path)) {
      CompleteGetEntryProperties(drive::FILE_ERROR_INVALID_OPERATION);
      return;
    }

    auto* drivefs_interface = integration_service->GetDriveFsInterface();
    if (!drivefs_interface) {
      CompleteGetEntryProperties(drive::FILE_ERROR_SERVICE_UNAVAILABLE);
      return;
    }

    drivefs_interface->GetMetadata(
        path, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                  base::BindOnce(
                      &SingleEntryPropertiesGetterForDriveFs::OnGetFileInfo,
                      weak_ptr_factory_.GetWeakPtr()),
                  drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
  }

  void OnGetFileInfo(drive::FileError error,
                     drivefs::mojom::FileMetadataPtr metadata) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!metadata) {
      CompleteGetEntryProperties(error);
      return;
    }

    properties_->size = std::make_unique<double>(metadata->size);
    properties_->present = std::make_unique<bool>(metadata->available_offline);
    properties_->dirty = std::make_unique<bool>(metadata->dirty);
    properties_->hosted = std::make_unique<bool>(
        metadata->type == drivefs::mojom::FileMetadata::Type::kHosted);
    properties_->available_offline = std::make_unique<bool>(
        metadata->available_offline || *properties_->hosted);
    properties_->available_when_metered = std::make_unique<bool>(
        metadata->available_offline || *properties_->hosted);
    properties_->pinned = std::make_unique<bool>(metadata->pinned);
    properties_->shared = std::make_unique<bool>(metadata->shared);
    properties_->starred = std::make_unique<bool>(metadata->starred);

    if (metadata->modification_time != base::Time()) {
      properties_->modification_time =
          std::make_unique<double>(metadata->modification_time.ToJsTime());
    }
    if (metadata->last_viewed_by_me_time != base::Time()) {
      properties_->modification_by_me_time =
          std::make_unique<double>(metadata->last_viewed_by_me_time.ToJsTime());
    }
    if (!metadata->content_mime_type.empty()) {
      properties_->content_mime_type =
          std::make_unique<std::string>(metadata->content_mime_type);
    }
    if (!metadata->custom_icon_url.empty()) {
      properties_->custom_icon_url =
          std::make_unique<std::string>(std::move(metadata->custom_icon_url));
    }
    if (!metadata->alternate_url.empty()) {
      properties_->alternate_url =
          std::make_unique<std::string>(std::move(metadata->alternate_url));
      properties_->share_url =
          GetShareUrlFromAlternateUrl(GURL(*properties_->alternate_url));
    }
    if (metadata->image_metadata) {
      if (metadata->image_metadata->height) {
        properties_->image_height =
            std::make_unique<int32_t>(metadata->image_metadata->height);
      }
      if (metadata->image_metadata->width) {
        properties_->image_width =
            std::make_unique<int32_t>(metadata->image_metadata->width);
      }
      if (metadata->image_metadata->rotation) {
        properties_->image_rotation =
            std::make_unique<int32_t>(metadata->image_metadata->rotation);
      }
    }

    properties_->can_delete =
        std::make_unique<bool>(metadata->capabilities->can_delete);
    properties_->can_rename =
        std::make_unique<bool>(metadata->capabilities->can_rename);
    properties_->can_add_children =
        std::make_unique<bool>(metadata->capabilities->can_add_children);

    // Only set the |can_copy| capability for hosted documents; for other files,
    // we must have read access, so |can_copy| is implicitly true.
    properties_->can_copy = std::make_unique<bool>(
        !*properties_->hosted || metadata->capabilities->can_copy);
    properties_->can_share =
        std::make_unique<bool>(metadata->capabilities->can_share);

    if (metadata->type != drivefs::mojom::FileMetadata::Type::kDirectory) {
      properties_->thumbnail_url = std::make_unique<std::string>(
          base::StrCat({"drivefs:", file_system_url_.ToGURL().spec()}));
      properties_->cropped_thumbnail_url =
          std::make_unique<std::string>(*properties_->thumbnail_url);
    }

    CompleteGetEntryProperties(drive::FILE_ERROR_OK);
  }

  void CompleteGetEntryProperties(drive::FileError error) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(callback_);

    std::move(callback_).Run(std::move(properties_),
                             drive::FileErrorToBaseFileError(error));
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }

  // Given parameters.
  ResultCallback callback_;
  const storage::FileSystemURL file_system_url_;
  Profile* const running_profile_;

  // Values used in the process.
  std::unique_ptr<EntryProperties> properties_;

  base::WeakPtrFactory<SingleEntryPropertiesGetterForDriveFs> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleEntryPropertiesGetterForDriveFs);
};

std::string MakeThumbnailDataUrlOnSequence(
    const std::vector<uint8_t>& png_data) {
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(png_data.data()),
                        png_data.size()),
      &encoded);
  return base::StrCat({"data:image/png;base64,", encoded});
}

drivefs::mojom::QueryParameters::QuerySource SearchDriveFs(
    scoped_refptr<ChromeAsyncExtensionFunction> function,
    drivefs::mojom::QueryParametersPtr query,
    bool filter_dirs,
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback);

void OnSearchDriveFs(
    scoped_refptr<ChromeAsyncExtensionFunction> function,
    drivefs::mojom::SearchQueryPtr search,
    drivefs::mojom::QueryParametersPtr query,
    bool filter_dirs,
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback,
    drive::FileError error,
    base::Optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  drive::DriveIntegrationService* integration_service =
      drive::util::GetIntegrationServiceByProfile(function->GetProfile());
  if (!integration_service) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (error == drive::FILE_ERROR_NO_CONNECTION &&
      query->query_source !=
          drivefs::mojom::QueryParameters::QuerySource::kLocalOnly) {
    // Retry with offline query.
    query->query_source =
        drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
    if (query->text_content) {
      // Full-text searches not supported offline.
      std::swap(query->text_content, query->title);
      query->text_content.reset();
    }
    SearchDriveFs(std::move(function), std::move(query), filter_dirs,
                  std::move(callback));
    return;
  }

  if (error != drive::FILE_ERROR_OK || !items.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }

  GURL url;
  file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
      function->GetProfile(), integration_service->GetMountPointPath(),
      function->extension_id(), &url);
  const auto fs_root = base::StrCat({url.spec(), "/"});
  const auto fs_name =
      integration_service->GetMountPointPath().BaseName().value();
  const base::FilePath root("/");

  auto result = std::make_unique<base::ListValue>();
  result->GetList().reserve(items->size());
  for (const auto& item : *items) {
    base::FilePath path;
    if (!root.AppendRelativePath(item->path, &path))
      path = item->path;
    base::DictionaryValue entry;
    entry.SetKey("fileSystemName", base::Value(fs_name));
    entry.SetKey("fileSystemRoot", base::Value(fs_root));
    entry.SetKey("fileFullPath", base::Value(item->path.AsUTF8Unsafe()));
    bool is_dir =
        item->metadata->type == drivefs::mojom::FileMetadata::Type::kDirectory;
    entry.SetKey("fileIsDirectory", base::Value(is_dir));
    if (!filter_dirs || !is_dir) {
      result->GetList().emplace_back(std::move(entry));
    }
  }

  std::move(callback).Run(std::move(result));
}

drivefs::mojom::QueryParameters::QuerySource SearchDriveFs(
    scoped_refptr<ChromeAsyncExtensionFunction> function,
    drivefs::mojom::QueryParametersPtr query,
    bool filter_dirs,
    base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback) {
  drive::DriveIntegrationService* const integration_service =
      drive::util::GetIntegrationServiceByProfile(function->GetProfile());
  drivefs::mojom::SearchQueryPtr search;
  integration_service->GetDriveFsInterface()->StartSearchQuery(
      mojo::MakeRequest(&search), query.Clone());
  drivefs::mojom::QueryParameters::QuerySource source = query->query_source;
  if (net::NetworkChangeNotifier::IsOffline() &&
      source != drivefs::mojom::QueryParameters::QuerySource::kLocalOnly) {
    // No point trying cloud query if we know we are offline.
    source = drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
    OnSearchDriveFs(std::move(function), std::move(search), std::move(query),
                    filter_dirs, std::move(callback),
                    drive::FILE_ERROR_NO_CONNECTION, {});
  } else {
    auto* raw_search = search.get();
    raw_search->GetNextPage(
        base::BindOnce(&OnSearchDriveFs, std::move(function), std::move(search),
                       std::move(query), filter_dirs, std::move(callback)));
  }
  return source;
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
}

FileManagerPrivateInternalGetEntryPropertiesFunction::
    ~FileManagerPrivateInternalGetEntryPropertiesFunction() = default;

bool FileManagerPrivateInternalGetEntryPropertiesFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using api::file_manager_private_internal::GetEntryProperties::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());

  properties_list_.resize(params->urls.size());
  const std::set<EntryPropertyName> names_as_set(params->names.begin(),
                                                 params->names.end());
  for (size_t i = 0; i < params->urls.size(); i++) {
    const GURL url = GURL(params->urls[i]);
    const storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(url);
    switch (file_system_url.type()) {
      case storage::kFileSystemTypeDrive:
        SingleEntryPropertiesGetterForDrive::Start(
            file_system_url.path(), names_as_set, GetProfile(),
            base::Bind(&FileManagerPrivateInternalGetEntryPropertiesFunction::
                           CompleteGetEntryProperties,
                       this, i, file_system_url));
        break;
      case storage::kFileSystemTypeProvided:
        SingleEntryPropertiesGetterForFileSystemProvider::Start(
            file_system_url, names_as_set,
            base::Bind(&FileManagerPrivateInternalGetEntryPropertiesFunction::
                           CompleteGetEntryProperties,
                       this, i, file_system_url));
        break;
      case storage::kFileSystemTypeDriveFs:
        SingleEntryPropertiesGetterForDriveFs::Start(
            file_system_url, GetProfile(),
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

  return true;
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

  results_ = extensions::api::file_manager_private_internal::
      GetEntryProperties::Results::Create(properties_list_);
  SendResponse(true);
}

bool FileManagerPrivateInternalPinDriveFileFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::PinDriveFile::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  switch (file_system_url.type()) {
    case storage::kFileSystemTypeDrive:
      return RunAsyncForDrive(url, params->pin);

    case storage::kFileSystemTypeDriveFs:
      return RunAsyncForDriveFs(file_system_url, params->pin);

    default:
      return false;
  }
}

bool FileManagerPrivateInternalPinDriveFileFunction::RunAsyncForDrive(
    const GURL& url,
    bool pin) {
  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system)  // |file_system| is NULL if Drive is disabled.
    return false;

  const base::FilePath drive_path =
      drive::util::ExtractDrivePath(file_manager::util::GetLocalPathFromURL(
          render_frame_host(), GetProfile(), url));
  if (pin) {
    file_system->Pin(
        drive_path,
        base::Bind(
            &FileManagerPrivateInternalPinDriveFileFunction::OnPinStateSet,
            this));
  } else {
    file_system->Unpin(
        drive_path,
        base::Bind(
            &FileManagerPrivateInternalPinDriveFileFunction::OnPinStateSet,
            this));
  }
  return true;
}

bool FileManagerPrivateInternalPinDriveFileFunction::RunAsyncForDriveFs(
    const storage::FileSystemURL& file_system_url,
    bool pin) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  base::FilePath path;
  if (!integration_service || !integration_service->GetRelativeDrivePath(
                                  file_system_url.path(), &path)) {
    return false;
  }

  auto* drivefs_interface = integration_service->GetDriveFsInterface();
  if (!drivefs_interface)
    return false;

  drivefs_interface->SetPinned(
      path, pin,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &FileManagerPrivateInternalPinDriveFileFunction::OnPinStateSet,
              this),
          drive::FILE_ERROR_SERVICE_UNAVAILABLE));
  return true;
}
void FileManagerPrivateInternalPinDriveFileFunction::OnPinStateSet(
    drive::FileError error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error == drive::FILE_ERROR_OK) {
    SendResponse(true);
  } else {
    SetError(drive::FileErrorToString(error));
    SendResponse(false);
  }
}

bool FileManagerPrivateInternalEnsureFileDownloadedFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  using extensions::api::file_manager_private_internal::EnsureFileDownloaded::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::FilePath drive_path =
      drive::util::ExtractDrivePath(file_manager::util::GetLocalPathFromURL(
          render_frame_host(), GetProfile(), GURL(params->url)));
  if (drive_path.empty()) {
    // Not under Drive. No need to fill the cache.
    SendResponse(true);
    return true;
  }

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system)  // |file_system| is NULL if Drive is disabled.
    return false;

  file_system->GetFile(
      drive_path,
      base::BindOnce(&FileManagerPrivateInternalEnsureFileDownloadedFunction::
                         OnDownloadFinished,
                     this));
  return true;
}

void FileManagerPrivateInternalEnsureFileDownloadedFunction::OnDownloadFinished(
    drive::FileError error,
    const base::FilePath& file_path,
    std::unique_ptr<drive::ResourceEntry> entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error == drive::FILE_ERROR_OK) {
    SendResponse(true);
  } else {
    SetError(drive::FileErrorToString(error));
    SendResponse(false);
  }
}

bool FileManagerPrivateInternalCancelFileTransfersFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::CancelFileTransfers::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  if (!integration_service || !integration_service->IsMounted())
    return false;

  drive::JobListInterface* const job_list = integration_service->job_list();
  DCHECK(job_list);
  const std::vector<drive::JobInfo> jobs = job_list->GetJobInfoList();

  // Create the mapping from file path to job ID.
  typedef std::map<base::FilePath, std::vector<drive::JobID>> PathToIdMap;
  PathToIdMap path_to_id_map;
  for (size_t i = 0; i < jobs.size(); ++i) {
    if (drive::IsActiveFileTransferJobInfo(jobs[i]))
      path_to_id_map[jobs[i].file_path].push_back(jobs[i].job_id);
  }

  for (size_t i = 0; i < params->urls.size(); ++i) {
    base::FilePath file_path = file_manager::util::GetLocalPathFromURL(
        render_frame_host(), GetProfile(), GURL(params->urls[i]));
    if (file_path.empty())
      continue;

    file_path = drive::util::ExtractDrivePath(file_path);
    DCHECK(file_path.empty());

    // Cancel all the jobs for the file.
    PathToIdMap::iterator it = path_to_id_map.find(file_path);
    if (it != path_to_id_map.end()) {
      for (size_t i = 0; i < it->second.size(); ++i)
        job_list->CancelJob(it->second[i]);
    }
  }

  SendResponse(true);
  return true;
}

bool FileManagerPrivateSearchDriveFunction::RunAsync() {
  using extensions::api::file_manager_private::SearchDrive::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!drive::util::GetIntegrationServiceByProfile(GetProfile())) {
    // |integration_service| is NULL if Drive is disabled or not mounted.
    return false;
  }

  operation_start_ = base::TimeTicks::Now();
  is_offline_ = net::NetworkChangeNotifier::IsOffline();

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (file_system) {
    file_system->Search(
        params->search_params.query, GURL(params->search_params.next_feed),
        base::BindOnce(&FileManagerPrivateSearchDriveFunction::OnSearch, this));
  } else {
    // |file_system| is NULL if the backend is DriveFs.
    auto query = drivefs::mojom::QueryParameters::New();
    query->text_content = params->search_params.query;
    is_offline_ =
        SearchDriveFs(
            this, std::move(query), false,
            base::BindOnce(
                &FileManagerPrivateSearchDriveFunction::OnSearchDriveFs,
                this)) ==
        drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  }

  return true;
}

void FileManagerPrivateSearchDriveFunction::OnSearchDriveFs(
    std::unique_ptr<base::ListValue> results) {
  if (!results) {
    UmaEmitSearchOutcome(
        false, !is_offline_,
        FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
        operation_start_);
    SendResponse(false);
    return;
  }
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetKey("entries", std::move(*results));
  // Search queries are capped at 100 of items anyway and pagination is
  // never actually used, so no need to fill this.
  result->SetKey("nextFeed", base::Value(""));
  SetResult(std::move(result));
  UmaEmitSearchOutcome(
      true, !is_offline_,
      FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
      operation_start_);
  SendResponse(true);
}

void FileManagerPrivateSearchDriveFunction::OnSearch(
    drive::FileError error,
    const GURL& next_link,
    std::unique_ptr<SearchResultInfoList> results) {
  if (error != drive::FILE_ERROR_OK) {
    UmaEmitSearchOutcome(
        false, !is_offline_,
        FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
        operation_start_);
    SendResponse(false);
    return;
  }

  // Outlives the following conversion, since the pointer is bound to the
  // callback.
  DCHECK(results.get());
  const SearchResultInfoList& results_ref = *results.get();

  ConvertSearchResultInfoListToEntryDefinitionList(
      GetProfile(), extension_->id(), results_ref,
      base::BindOnce(
          &FileManagerPrivateSearchDriveFunction::OnEntryDefinitionList, this,
          next_link, std::move(results)));
}

void FileManagerPrivateSearchDriveFunction::OnEntryDefinitionList(
    const GURL& next_link,
    std::unique_ptr<SearchResultInfoList> search_result_info_list,
    std::unique_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_EQ(search_result_info_list->size(), entry_definition_list->size());
  auto entries = file_manager::util::ConvertEntryDefinitionListToListValue(
      *entry_definition_list);

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->Set("entries", std::move(entries));
  result->SetString("nextFeed", next_link.spec());

  SetResult(std::move(result));
  UmaEmitSearchOutcome(
      true, !is_offline_,
      FileManagerPrivateSearchDriveMetadataFunction::SearchType::kText,
      operation_start_);
  SendResponse(true);
}

bool FileManagerPrivateSearchDriveMetadataFunction::RunAsync() {
  using api::file_manager_private::SearchDriveMetadata::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::EventLogger* logger = file_manager::util::GetLogger(GetProfile());
  if (logger) {
    logger->Log(
        logging::LOG_INFO, "%s[%d] called. (types: '%s', maxResults: '%d')",
        name(), request_id(),
        api::file_manager_private::ToString(params->search_params.types),
        params->search_params.max_results);
  }
  set_log_on_completion(true);

  drive::DriveIntegrationService* const integration_service =
      drive::util::GetIntegrationServiceByProfile(GetProfile());
  if (!integration_service) {
    // |integration_service| is NULL if Drive is disabled or not mounted.
    return false;
  }

  operation_start_ = base::TimeTicks::Now();
  is_offline_ = true;  // Legacy search is assumed offline always.

  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  // |file_system| is NULL if the backend is DriveFs, otherwise it's legacy
  // sync client.
  if (file_system) {
    int options = -1;
    switch (params->search_params.types) {
      case api::file_manager_private::SEARCH_TYPE_EXCLUDE_DIRECTORIES:
        options = drive::SEARCH_METADATA_EXCLUDE_DIRECTORIES;
        search_type_ = SearchType::kText;
        break;
      case api::file_manager_private::SEARCH_TYPE_SHARED_WITH_ME:
        options = drive::SEARCH_METADATA_SHARED_WITH_ME;
        search_type_ = SearchType::kSharedWithMe;
        break;
      case api::file_manager_private::SEARCH_TYPE_OFFLINE:
        options = drive::SEARCH_METADATA_OFFLINE;
        search_type_ = SearchType::kOffline;
        break;
      case api::file_manager_private::SEARCH_TYPE_ALL:
        options = drive::SEARCH_METADATA_ALL;
        search_type_ = SearchType::kText;
        break;
      default:
        return false;
    }
    file_system->SearchMetadata(
        params->search_params.query, options, params->search_params.max_results,
        drive::MetadataSearchOrder::LAST_ACCESSED,
        base::BindOnce(
            &FileManagerPrivateSearchDriveMetadataFunction::OnSearchMetadata,
            this));
  } else {
    auto query = drivefs::mojom::QueryParameters::New();
    query->sort_field =
        drivefs::mojom::QueryParameters::SortField::kLastModified;
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
        return false;
    }
    is_offline_ =
        SearchDriveFs(
            this, std::move(query), filter_dirs,
            base::BindOnce(
                &FileManagerPrivateSearchDriveMetadataFunction::OnSearchDriveFs,
                this, params->search_params.query)) ==
        drivefs::mojom::QueryParameters::QuerySource::kLocalOnly;
  }

  return true;
}

void FileManagerPrivateSearchDriveMetadataFunction::OnSearchDriveFs(
    const std::string& query_text,
    std::unique_ptr<base::ListValue> results) {
  if (!results) {
    UmaEmitSearchOutcome(false, !is_offline_, search_type_, operation_start_);
    SendResponse(false);
    return;
  }

  std::vector<base::string16> keywords =
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
  results_list->GetList().reserve(results->GetList().size());
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
    dict.SetKey("entry", std::move(entry));
    dict.SetKey("highlightedBaseName", base::Value(highlight));
    results_list->GetList().emplace_back(std::move(dict));
  }

  SetResult(std::move(results_list));
  UmaEmitSearchOutcome(true, !is_offline_, search_type_, operation_start_);
  SendResponse(true);
}

void FileManagerPrivateSearchDriveMetadataFunction::OnSearchMetadata(
    drive::FileError error,
    std::unique_ptr<drive::MetadataSearchResultVector> results) {
  if (error != drive::FILE_ERROR_OK) {
    UmaEmitSearchOutcome(false, !is_offline_, search_type_, operation_start_);
    SendResponse(false);
    return;
  }

  // Outlives the following conversion, since the pointer is bound to the
  // callback.
  DCHECK(results.get());
  const drive::MetadataSearchResultVector& results_ref = *results.get();

  ConvertSearchResultInfoListToEntryDefinitionList(
      GetProfile(), extension_->id(), results_ref,
      base::BindOnce(
          &FileManagerPrivateSearchDriveMetadataFunction::OnEntryDefinitionList,
          this, std::move(results)));
}

void FileManagerPrivateSearchDriveMetadataFunction::OnEntryDefinitionList(
    std::unique_ptr<drive::MetadataSearchResultVector> search_result_info_list,
    std::unique_ptr<EntryDefinitionList> entry_definition_list) {
  DCHECK_EQ(search_result_info_list->size(), entry_definition_list->size());
  std::unique_ptr<base::ListValue> results_list(new base::ListValue());

  // Convert Drive files to something File API stack can understand.  See
  // file_browser_handler_custom_bindings.cc and
  // file_manager_private_custom_bindings.js for how this is magically
  // converted to a FileEntry.
  for (size_t i = 0; i < entry_definition_list->size(); ++i) {
    auto result_dict = std::make_unique<base::DictionaryValue>();

    // FileEntry fields.
    auto entry = file_manager::util::ConvertEntryDefinitionToValue(
        entry_definition_list->at(i));

    result_dict->Set("entry", std::move(entry));
    result_dict->SetString(
        "highlightedBaseName",
        search_result_info_list->at(i).highlighted_base_name);
    results_list->Append(std::move(result_dict));
  }

  SetResult(std::move(results_list));
  UmaEmitSearchOutcome(true, !is_offline_, search_type_, operation_start_);
  SendResponse(true);
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetDriveConnectionStateFunction::Run() {
  api::file_manager_private::DriveConnectionState result;

  switch (drive::util::GetDriveConnectionStatus(
      Profile::FromBrowserContext(browser_context()))) {
    case drive::util::DRIVE_DISCONNECTED_NOSERVICE:
      result.type = kDriveConnectionTypeOffline;
      result.reason =
          std::make_unique<std::string>(kDriveConnectionReasonNoService);
      break;
    case drive::util::DRIVE_DISCONNECTED_NONETWORK:
      result.type = kDriveConnectionTypeOffline;
      result.reason =
          std::make_unique<std::string>(kDriveConnectionReasonNoNetwork);
      break;
    case drive::util::DRIVE_DISCONNECTED_NOTREADY:
      result.type = kDriveConnectionTypeOffline;
      result.reason =
          std::make_unique<std::string>(kDriveConnectionReasonNotReady);
      break;
    case drive::util::DRIVE_CONNECTED_METERED:
      result.type = kDriveConnectionTypeMetered;
      break;
    case drive::util::DRIVE_CONNECTED:
      result.type = kDriveConnectionTypeOnline;
      break;
  }

  result.has_cellular_network_access =
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->FirstNetworkByType(chromeos::NetworkTypePattern::Mobile());

  drive::EventLogger* logger = file_manager::util::GetLogger(
      Profile::FromBrowserContext(browser_context()));
  if (logger)
    logger->Log(logging::LOG_INFO, "%s succeeded.", name());
  return RespondNow(ArgumentList(
      api::file_manager_private::GetDriveConnectionState::Results::Create(
          result)));
}

bool FileManagerPrivateRequestAccessTokenFunction::RunAsync() {
  using extensions::api::file_manager_private::RequestAccessToken::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::DriveServiceInterface* const drive_service =
      drive::util::GetDriveServiceByProfile(GetProfile());

  if (!drive_service) {
    // DriveService is not available.
    SetResult(std::make_unique<base::Value>(std::string()));
    SendResponse(true);
    return true;
  }

  // If refreshing is requested, then clear the token to refetch it.
  if (params->refresh)
    drive_service->ClearAccessToken();

  // Retrieve the cached auth token (if available), otherwise the AuthService
  // instance will try to refetch it.
  drive_service->RequestAccessToken(
      base::Bind(&FileManagerPrivateRequestAccessTokenFunction::
                      OnAccessTokenFetched, this));
  return true;
}

void FileManagerPrivateRequestAccessTokenFunction::OnAccessTokenFetched(
    google_apis::DriveApiErrorCode code,
    const std::string& access_token) {
  SetResult(std::make_unique<base::Value>(access_token));
  SendResponse(true);
}

bool FileManagerPrivateInternalRequestDriveShareFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::RequestDriveShare::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), GetProfile(), GURL(params->url));
  const base::FilePath drive_path = drive::util::ExtractDrivePath(path);
  Profile* const owner_profile = drive::util::ExtractProfileFromPath(path);

  if (!owner_profile)
    return false;

  drive::FileSystemInterface* const owner_file_system =
      drive::util::GetFileSystemByProfile(owner_profile);
  if (!owner_file_system)
    return false;

  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(GetProfile());
  if (!user || !user->is_logged_in())
    return false;

  google_apis::drive::PermissionRole role =
      google_apis::drive::PERMISSION_ROLE_READER;
  switch (params->share_type) {
    case api::file_manager_private::DRIVE_SHARE_TYPE_NONE:
      NOTREACHED();
      return false;
    case api::file_manager_private::DRIVE_SHARE_TYPE_CAN_EDIT:
      role = google_apis::drive::PERMISSION_ROLE_WRITER;
      break;
    case api::file_manager_private::DRIVE_SHARE_TYPE_CAN_COMMENT:
      role = google_apis::drive::PERMISSION_ROLE_COMMENTER;
      break;
    case api::file_manager_private::DRIVE_SHARE_TYPE_CAN_VIEW:
      role = google_apis::drive::PERMISSION_ROLE_READER;
      break;
  }

  // Share |drive_path| in |owner_file_system| to
  // |user->GetAccountId().GetUserEmail()|.
  owner_file_system->AddPermission(
      drive_path, user->GetAccountId().GetUserEmail(), role,
      base::Bind(
          &FileManagerPrivateInternalRequestDriveShareFunction::OnAddPermission,
          this));
  return true;
}

void FileManagerPrivateInternalRequestDriveShareFunction::OnAddPermission(
    drive::FileError error) {
  SendResponse(error == drive::FILE_ERROR_OK);
}

FileManagerPrivateInternalGetDownloadUrlFunction::
    FileManagerPrivateInternalGetDownloadUrlFunction() = default;

FileManagerPrivateInternalGetDownloadUrlFunction::
    ~FileManagerPrivateInternalGetDownloadUrlFunction() = default;

bool FileManagerPrivateInternalGetDownloadUrlFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::GetDownloadUrl::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  switch (file_system_url.type()) {
    case storage::kFileSystemTypeDrive:
      return RunAsyncForDrive(url);
    case storage::kFileSystemTypeDriveFs:
      return RunAsyncForDriveFs(file_system_url);
    default:
      return false;
  }
}

bool FileManagerPrivateInternalGetDownloadUrlFunction::RunAsyncForDrive(
    const GURL& url) {
  // Start getting the file info.
  drive::FileSystemInterface* const file_system =
      drive::util::GetFileSystemByProfile(GetProfile());
  if (!file_system) {
    // |file_system| is NULL if Drive is disabled or not mounted.
    SetError("Drive is disabled or not mounted.");
    // Intentionally returns a blank.
    SetResult(std::make_unique<base::Value>(std::string()));
    return false;
  }

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), GetProfile(), url);
  if (!drive::util::IsUnderDriveMountPoint(path)) {
    SetError("The given file is not in Drive.");
    // Intentionally returns a blank.
    SetResult(std::make_unique<base::Value>(std::string()));
    return false;
  }
  base::FilePath file_path = drive::util::ExtractDrivePath(path);

  file_system->GetResourceEntry(
      file_path,
      base::BindOnce(
          &FileManagerPrivateInternalGetDownloadUrlFunction::OnGetResourceEntry,
          this));
  return true;
}

void FileManagerPrivateInternalGetDownloadUrlFunction::OnGetResourceEntry(
    drive::FileError error,
    std::unique_ptr<drive::ResourceEntry> entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (error != drive::FILE_ERROR_OK) {
    OnGotDownloadUrl(GURL());
    return;
  }

  DriveApiUrlGenerator url_generator(
      (GURL(google_apis::DriveApiUrlGenerator::kBaseUrlForProduction)),
      (GURL(
          google_apis::DriveApiUrlGenerator::kBaseThumbnailUrlForProduction)));
  OnGotDownloadUrl(url_generator.GenerateDownloadFileUrl(entry->resource_id()));
}

void FileManagerPrivateInternalGetDownloadUrlFunction::OnGotDownloadUrl(
    GURL download_url) {
  if (download_url.is_empty()) {
    SetError("Download Url for this item is not available.");
    // Intentionally returns a blank.
    SetResult(std::make_unique<base::Value>(std::string()));
    SendResponse(false);
    return;
  }
  download_url_ = std::move(download_url);
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  const std::string& account_id = identity_manager->GetPrimaryAccountId();
  std::vector<std::string> scopes;
  scopes.emplace_back("https://www.googleapis.com/auth/drive.readonly");

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(GetProfile())
          ->GetURLLoaderFactoryForBrowserProcess();
  auth_service_ = std::make_unique<google_apis::AuthService>(
      identity_manager, account_id, url_loader_factory, scopes);
  auth_service_->StartAuthentication(base::Bind(
      &FileManagerPrivateInternalGetDownloadUrlFunction::OnTokenFetched, this));
}

void FileManagerPrivateInternalGetDownloadUrlFunction::OnTokenFetched(
    google_apis::DriveApiErrorCode code,
    const std::string& access_token) {
  if (code != google_apis::HTTP_SUCCESS) {
    SetError("Not able to fetch the token.");
    // Intentionally returns a blank.
    SetResult(std::make_unique<base::Value>(std::string()));
    SendResponse(false);
    return;
  }

  SetResult(std::make_unique<base::Value>(
      download_url_.Resolve("?alt=media&access_token=" + access_token).spec()));

  SendResponse(true);
}

bool FileManagerPrivateInternalGetDownloadUrlFunction::RunAsyncForDriveFs(
    const storage::FileSystemURL& file_system_url) {
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  base::FilePath path;
  if (!integration_service || !integration_service->GetRelativeDrivePath(
                                  file_system_url.path(), &path)) {
    return false;
  }

  auto* drivefs_interface = integration_service->GetDriveFsInterface();
  if (!drivefs_interface)
    return false;

  drivefs_interface->GetMetadata(
      path,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &FileManagerPrivateInternalGetDownloadUrlFunction::OnGotMetadata,
              this),
          drive::FILE_ERROR_SERVICE_UNAVAILABLE, nullptr));
  return true;
}

void FileManagerPrivateInternalGetDownloadUrlFunction::OnGotMetadata(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  OnGotDownloadUrl(metadata ? GURL(metadata->download_url) : GURL());
}

FileManagerPrivateInternalGetThumbnailFunction::
    FileManagerPrivateInternalGetThumbnailFunction() = default;

FileManagerPrivateInternalGetThumbnailFunction::
    ~FileManagerPrivateInternalGetThumbnailFunction() = default;

// ChromeAsyncExtensionFunction overrides.
bool FileManagerPrivateInternalGetThumbnailFunction::RunAsync() {
  using extensions::api::file_manager_private_internal::GetThumbnail::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          GetProfile(), render_frame_host());
  const GURL url = GURL(params->url);
  const storage::FileSystemURL file_system_url =
      file_system_context->CrackURL(url);

  if (file_system_url.type() != storage::kFileSystemTypeDriveFs) {
    return false;
  }
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(GetProfile());
  base::FilePath path;
  if (!integration_service || !integration_service->GetRelativeDrivePath(
                                  file_system_url.path(), &path)) {
    return false;
  }
  auto* drivefs_interface = integration_service->GetDriveFsInterface();
  if (!drivefs_interface) {
    return false;
  }
  drivefs_interface->GetThumbnail(
      path, params->crop_to_square,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &FileManagerPrivateInternalGetThumbnailFunction::GotThumbnail,
              this),
          base::Optional<std::vector<uint8_t>>()));
  return true;
}

void FileManagerPrivateInternalGetThumbnailFunction::GotThumbnail(
    const base::Optional<std::vector<uint8_t>>& data) {
  if (!data) {
    SetResult(std::make_unique<base::Value>(""));
    SendResponse(true);
    return;
  }
  base::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&MakeThumbnailDataUrlOnSequence, *data),
      base::BindOnce(
          &FileManagerPrivateInternalGetThumbnailFunction::SendEncodedThumbnail,
          this));
}

void FileManagerPrivateInternalGetThumbnailFunction::SendEncodedThumbnail(
    std::string thumbnail_data_url) {
  SetResult(std::make_unique<base::Value>(std::move(thumbnail_data_url)));
  SendResponse(true);
}

}  // namespace extensions
