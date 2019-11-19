// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/apps/platform_apps/api/media_galleries/media_galleries_api.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/media_galleries/blob_data_source_factory.h"
#include "chrome/browser/apps/platform_apps/api/media_galleries/media_galleries_api_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media_galleries/gallery_watch_manager.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/media_galleries/media_galleries_permission_controller.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/apps/platform_apps/api/media_galleries.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/media_gallery_util/public/cpp/safe_media_metadata_parser.h"
#include "components/storage_monitor/storage_info.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/blob_holder.h"
#include "extensions/browser/blob_reader.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/mime_sniffer.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using storage_monitor::MediaStorageUtil;
using storage_monitor::StorageInfo;

namespace chrome_apps {
namespace api {

namespace MediaGalleries = media_galleries;
namespace GetMediaFileSystems = MediaGalleries::GetMediaFileSystems;
namespace AddGalleryWatch = MediaGalleries::AddGalleryWatch;
namespace RemoveGalleryWatch = MediaGalleries::RemoveGalleryWatch;

namespace {

const char kDisallowedByPolicy[] =
    "Media Galleries API is disallowed by policy: ";
const char kInvalidGalleryIdMsg[] = "Invalid gallery id.";
const char kMissingEventListener[] = "Missing event listener registration.";

const char kDeviceIdKey[] = "deviceId";
const char kGalleryIdKey[] = "galleryId";
const char kIsAvailableKey[] = "isAvailable";
const char kIsMediaDeviceKey[] = "isMediaDevice";
const char kIsRemovableKey[] = "isRemovable";
const char kNameKey[] = "name";

const char kMetadataKey[] = "metadata";
const char kAttachedImagesBlobInfoKey[] = "attachedImagesBlobInfo";
const char kBlobUUIDKey[] = "blobUUID";
const char kMediaGalleriesApiTypeKey[] = "type";
const char kSizeKey[] = "size";

const char kInvalidGalleryId[] = "-1";

MediaFileSystemRegistry* media_file_system_registry() {
  return g_browser_process->media_file_system_registry();
}

GalleryWatchManager* gallery_watch_manager() {
  return media_file_system_registry()->gallery_watch_manager();
}

// Checks whether the MediaGalleries API is currently accessible (it may be
// disallowed even if an extension has the requisite permission). Then
// initializes the MediaGalleriesPreferences
bool Setup(Profile* profile, std::string* error, base::Closure callback) {
  if (!ChromeSelectFilePolicy::FileSelectDialogsAllowed()) {
    *error =
        std::string(kDisallowedByPolicy) + prefs::kAllowFileSelectionDialogs;
    return false;
  }

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(profile);
  preferences->EnsureInitialized(callback);
  return true;
}

// Returns true and sets |gallery_file_path| and |gallery_pref_id| if the
// |gallery_id| is valid and returns false otherwise.
bool GetGalleryFilePathAndId(const std::string& gallery_id,
                             Profile* profile,
                             const extensions::Extension* extension,
                             base::FilePath* gallery_file_path,
                             MediaGalleryPrefId* gallery_pref_id) {
  MediaGalleryPrefId pref_id;
  if (!base::StringToUint64(gallery_id, &pref_id))
    return false;
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  base::FilePath file_path(
      preferences->LookUpGalleryPathForExtension(pref_id, extension, false));
  if (file_path.empty())
    return false;
  *gallery_pref_id = pref_id;
  *gallery_file_path = file_path;
  return true;
}

base::ListValue* ConstructFileSystemList(
    content::RenderFrameHost* rfh,
    const extensions::Extension* extension,
    const std::vector<MediaFileSystemInfo>& filesystems) {
  if (!rfh)
    return NULL;

  MediaGalleriesPermission::CheckParam read_param(
      MediaGalleriesPermission::kReadPermission);
  const extensions::PermissionsData* permissions_data =
      extension->permissions_data();
  bool has_read_permission = permissions_data->CheckAPIPermissionWithParam(
      extensions::APIPermission::kMediaGalleries, &read_param);
  MediaGalleriesPermission::CheckParam copy_to_param(
      MediaGalleriesPermission::kCopyToPermission);
  bool has_copy_to_permission = permissions_data->CheckAPIPermissionWithParam(
      extensions::APIPermission::kMediaGalleries, &copy_to_param);
  MediaGalleriesPermission::CheckParam delete_param(
      MediaGalleriesPermission::kDeletePermission);
  bool has_delete_permission = permissions_data->CheckAPIPermissionWithParam(
      extensions::APIPermission::kMediaGalleries, &delete_param);

  const int child_id = rfh->GetProcess()->GetID();
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < filesystems.size(); ++i) {
    std::unique_ptr<base::DictionaryValue> file_system_dict_value(
        new base::DictionaryValue());

    // Send the file system id so the renderer can create a valid FileSystem
    // object.
    file_system_dict_value->SetKey("fsid", base::Value(filesystems[i].fsid));

    file_system_dict_value->SetKey(kNameKey, base::Value(filesystems[i].name));
    file_system_dict_value->SetKey(
        kGalleryIdKey,
        base::Value(base::NumberToString(filesystems[i].pref_id)));
    if (!filesystems[i].transient_device_id.empty()) {
      file_system_dict_value->SetKey(
          kDeviceIdKey, base::Value(filesystems[i].transient_device_id));
    }
    file_system_dict_value->SetKey(kIsRemovableKey,
                                   base::Value(filesystems[i].removable));
    file_system_dict_value->SetKey(kIsMediaDeviceKey,
                                   base::Value(filesystems[i].media_device));
    file_system_dict_value->SetKey(kIsAvailableKey, base::Value(true));

    list->Append(std::move(file_system_dict_value));

    if (filesystems[i].path.empty())
      continue;

    if (has_read_permission) {
      content::ChildProcessSecurityPolicy* policy =
          content::ChildProcessSecurityPolicy::GetInstance();
      policy->GrantReadFile(child_id, filesystems[i].path);
      if (has_delete_permission) {
        policy->GrantDeleteFrom(child_id, filesystems[i].path);
        if (has_copy_to_permission) {
          policy->GrantCopyInto(child_id, filesystems[i].path);
        }
      }
    }
  }

  return list.release();
}

class SelectDirectoryDialog : public ui::SelectFileDialog::Listener,
                              public base::RefCounted<SelectDirectoryDialog> {
 public:
  // Selected file path, or an empty path if the user canceled.
  typedef base::Callback<void(const base::FilePath&)> Callback;

  SelectDirectoryDialog(WebContents* web_contents, const Callback& callback)
      : web_contents_(web_contents), callback_(callback) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  }

  void Show(const base::FilePath& default_path) {
    AddRef();  // Balanced in the two reachable listener outcomes.
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_FOLDER,
        l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY_TITLE),
        default_path, NULL, 0, base::FilePath::StringType(),
        platform_util::GetTopLevel(web_contents_->GetNativeView()), NULL);
  }

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    callback_.Run(path);
    Release();  // Balanced in Show().
  }

  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override {
    NOTREACHED() << "Should not be able to select multiple files";
  }

  void FileSelectionCanceled(void* params) override {
    callback_.Run(base::FilePath());
    Release();  // Balanced in Show().
  }

 private:
  friend class base::RefCounted<SelectDirectoryDialog>;
  ~SelectDirectoryDialog() override {}

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  WebContents* web_contents_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(SelectDirectoryDialog);
};

// Returns a web contents to use as the source for a prompt showing to the user.
// The web contents has to support modal dialogs, so it can't be the app's
// background page.
content::WebContents* GetWebContentsForPrompt(
    content::WebContents* sender_web_contents,
    content::BrowserContext* browser_context,
    const std::string& app_id) {
  // Check if the sender web contents supports modal dialogs.
  if (sender_web_contents &&
      web_modal::WebContentsModalDialogManager::FromWebContents(
          sender_web_contents)) {
    return sender_web_contents;
  }
  // Otherwise, check for the current app window for the app (app windows
  // support modal dialogs).
  if (!app_id.empty()) {
    extensions::AppWindow* window =
        extensions::AppWindowRegistry::Get(browser_context)
            ->GetCurrentAppWindowForApp(app_id);
    if (window)
      return window->web_contents();
  }
  return nullptr;
}

}  // namespace

MediaGalleriesEventRouter::MediaGalleriesEventRouter(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  extensions::EventRouter::Get(profile_)->RegisterObserver(
      this, MediaGalleries::OnGalleryChanged::kEventName);

  gallery_watch_manager()->AddObserver(profile_, this);
}

MediaGalleriesEventRouter::~MediaGalleriesEventRouter() {}

void MediaGalleriesEventRouter::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_ptr_factory_.InvalidateWeakPtrs();

  extensions::EventRouter::Get(profile_)->UnregisterObserver(this);

  gallery_watch_manager()->RemoveObserver(profile_);
}

static base::LazyInstance<extensions::BrowserContextKeyedAPIFactory<
    MediaGalleriesEventRouter>>::DestructorAtExit
    g_media_galleries_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
extensions::BrowserContextKeyedAPIFactory<MediaGalleriesEventRouter>*
MediaGalleriesEventRouter::GetFactoryInstance() {
  return g_media_galleries_api_factory.Pointer();
}

// static
MediaGalleriesEventRouter* MediaGalleriesEventRouter::Get(
    content::BrowserContext* context) {
  DCHECK(media_file_system_registry()
             ->GetPreferences(Profile::FromBrowserContext(context))
             ->IsInitialized());
  return extensions::BrowserContextKeyedAPIFactory<
      MediaGalleriesEventRouter>::Get(context);
}

bool MediaGalleriesEventRouter::ExtensionHasGalleryChangeListener(
    const std::string& extension_id) const {
  return extensions::EventRouter::Get(profile_)->ExtensionHasEventListener(
      extension_id, MediaGalleries::OnGalleryChanged::kEventName);
}

void MediaGalleriesEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    extensions::events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  extensions::EventRouter* router = extensions::EventRouter::Get(profile_);
  if (!router->ExtensionHasEventListener(extension_id, event_name))
    return;

  std::unique_ptr<extensions::Event> event(new extensions::Event(
      histogram_value, event_name, std::move(event_args)));
  router->DispatchEventToExtension(extension_id, std::move(event));
}

void MediaGalleriesEventRouter::OnGalleryChanged(
    const std::string& extension_id,
    MediaGalleryPrefId gallery_id) {
  MediaGalleries::GalleryChangeDetails details;
  details.type = MediaGalleries::GALLERY_CHANGE_TYPE_CONTENTS_CHANGED;
  details.gallery_id = base::NumberToString(gallery_id);
  DispatchEventToExtension(
      extension_id, extensions::events::MEDIA_GALLERIES_ON_GALLERY_CHANGED,
      MediaGalleries::OnGalleryChanged::kEventName,
      MediaGalleries::OnGalleryChanged::Create(details));
}

void MediaGalleriesEventRouter::OnGalleryWatchDropped(
    const std::string& extension_id,
    MediaGalleryPrefId gallery_id) {
  MediaGalleries::GalleryChangeDetails details;
  details.type = MediaGalleries::GALLERY_CHANGE_TYPE_WATCH_DROPPED;
  details.gallery_id = gallery_id;
  DispatchEventToExtension(
      extension_id, extensions::events::MEDIA_GALLERIES_ON_GALLERY_CHANGED,
      MediaGalleries::OnGalleryChanged::kEventName,
      MediaGalleries::OnGalleryChanged::Create(details));
}

void MediaGalleriesEventRouter::OnListenerRemoved(
    const extensions::EventListenerInfo& details) {
  if (details.event_name == MediaGalleries::OnGalleryChanged::kEventName &&
      !ExtensionHasGalleryChangeListener(details.extension_id)) {
    gallery_watch_manager()->RemoveAllWatches(profile_, details.extension_id);
  }
}

///////////////////////////////////////////////////////////////////////////////
//               MediaGalleriesGetMediaFileSystemsFunction                   //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesGetMediaFileSystemsFunction::
    ~MediaGalleriesGetMediaFileSystemsFunction() {}

bool MediaGalleriesGetMediaFileSystemsFunction::RunAsync() {
  ::media_galleries::UsageCount(::media_galleries::GET_MEDIA_FILE_SYSTEMS);
  std::unique_ptr<GetMediaFileSystems::Params> params(
      GetMediaFileSystems::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  MediaGalleries::GetMediaFileSystemsInteractivity interactive =
      MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NO;
  if (params->details.get() &&
      params->details->interactive !=
          MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NONE) {
    interactive = params->details->interactive;
  }

  return Setup(
      GetProfile(), &error_,
      base::Bind(&MediaGalleriesGetMediaFileSystemsFunction::OnPreferencesInit,
                 this, interactive));
}

void MediaGalleriesGetMediaFileSystemsFunction::OnPreferencesInit(
    MediaGalleries::GetMediaFileSystemsInteractivity interactive) {
  switch (interactive) {
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_YES: {
      // The MediaFileSystemRegistry only updates preferences for extensions
      // that it knows are in use. Since this may be the first call to
      // chrome.getMediaFileSystems for this extension, call
      // GetMediaFileSystemsForExtension() here solely so that
      // MediaFileSystemRegistry will send preference changes.
      GetMediaFileSystemsForExtension(base::Bind(
          &MediaGalleriesGetMediaFileSystemsFunction::AlwaysShowDialog, this));
      return;
    }
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_IF_NEEDED: {
      GetMediaFileSystemsForExtension(base::Bind(
          &MediaGalleriesGetMediaFileSystemsFunction::ShowDialogIfNoGalleries,
          this));
      return;
    }
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NO:
      GetAndReturnGalleries();
      return;
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NONE:
      NOTREACHED();
  }
  SendResponse(false);
}

void MediaGalleriesGetMediaFileSystemsFunction::AlwaysShowDialog(
    const std::vector<MediaFileSystemInfo>& /*filesystems*/) {
  ShowDialog();
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialogIfNoGalleries(
    const std::vector<MediaFileSystemInfo>& filesystems) {
  if (filesystems.empty())
    ShowDialog();
  else
    ReturnGalleries(filesystems);
}

void MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries() {
  GetMediaFileSystemsForExtension(base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::ReturnGalleries, this));
}

void MediaGalleriesGetMediaFileSystemsFunction::ReturnGalleries(
    const std::vector<MediaFileSystemInfo>& filesystems) {
  std::unique_ptr<base::ListValue> list(
      ConstructFileSystemList(render_frame_host(), extension(), filesystems));
  if (!list.get()) {
    SendResponse(false);
    return;
  }

  // The custom JS binding will use this list to create DOMFileSystem objects.
  SetResult(std::move(list));
  SendResponse(true);
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialog() {
  ::media_galleries::UsageCount(::media_galleries::SHOW_DIALOG);
  WebContents* contents = GetWebContentsForPrompt(
      GetSenderWebContents(), browser_context(), extension()->id());
  if (!contents) {
    SendResponse(false);
    return;
  }

  // Controller will delete itself.
  base::Closure cb = base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries, this);
  new MediaGalleriesPermissionController(contents, *extension(), cb);
}

void MediaGalleriesGetMediaFileSystemsFunction::GetMediaFileSystemsForExtension(
    const MediaFileSystemsCallback& cb) {
  if (!render_frame_host()) {
    cb.Run(std::vector<MediaFileSystemInfo>());
    return;
  }
  MediaFileSystemRegistry* registry = media_file_system_registry();
  DCHECK(registry->GetPreferences(GetProfile())->IsInitialized());
  registry->GetMediaFileSystemsForExtension(GetSenderWebContents(), extension(),
                                            cb);
}

///////////////////////////////////////////////////////////////////////////////
//               MediaGalleriesAddUserSelectedFolderFunction                 //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesAddUserSelectedFolderFunction::
    ~MediaGalleriesAddUserSelectedFolderFunction() {}

bool MediaGalleriesAddUserSelectedFolderFunction::RunAsync() {
  ::media_galleries::UsageCount(::media_galleries::ADD_USER_SELECTED_FOLDER);
  return Setup(
      GetProfile(), &error_,
      base::Bind(
          &MediaGalleriesAddUserSelectedFolderFunction::OnPreferencesInit,
          this));
}

void MediaGalleriesAddUserSelectedFolderFunction::OnPreferencesInit() {
  const std::string& app_id = extension()->id();
  WebContents* contents = GetWebContentsForPrompt(GetSenderWebContents(),
                                                  browser_context(), app_id);
  if (!contents) {
    SendResponse(false);
    return;
  }

  if (!user_gesture()) {
    OnDirectorySelected(base::FilePath());
    return;
  }

  base::FilePath last_used_path =
      extensions::file_system_api::GetLastChooseEntryDirectory(
          extensions::ExtensionPrefs::Get(browser_context()), app_id);
  SelectDirectoryDialog::Callback callback = base::Bind(
      &MediaGalleriesAddUserSelectedFolderFunction::OnDirectorySelected, this);
  scoped_refptr<SelectDirectoryDialog> select_directory_dialog =
      new SelectDirectoryDialog(contents, callback);
  select_directory_dialog->Show(last_used_path);
}

void MediaGalleriesAddUserSelectedFolderFunction::OnDirectorySelected(
    const base::FilePath& selected_directory) {
  if (selected_directory.empty()) {
    // User cancelled case.
    GetMediaFileSystemsForExtension(base::Bind(
        &MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId,
        this, kInvalidMediaGalleryPrefId));
    return;
  }

  extensions::file_system_api::SetLastChooseEntryDirectory(
      extensions::ExtensionPrefs::Get(GetProfile()), extension()->id(),
      selected_directory);

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(GetProfile());
  MediaGalleryPrefId pref_id = preferences->AddGalleryByPath(
      selected_directory, MediaGalleryPrefInfo::kUserAdded);
  preferences->SetGalleryPermissionForExtension(*extension(), pref_id, true);

  GetMediaFileSystemsForExtension(base::Bind(
      &MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId, this,
      pref_id));
}

void MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId(
    MediaGalleryPrefId pref_id,
    const std::vector<MediaFileSystemInfo>& filesystems) {
  std::unique_ptr<base::ListValue> list(
      ConstructFileSystemList(render_frame_host(), extension(), filesystems));
  if (!list.get()) {
    SendResponse(false);
    return;
  }

  int index = -1;
  if (pref_id != kInvalidMediaGalleryPrefId) {
    for (size_t i = 0; i < filesystems.size(); ++i) {
      if (filesystems[i].pref_id == pref_id) {
        index = i;
        break;
      }
    }
  }
  std::unique_ptr<base::DictionaryValue> results(new base::DictionaryValue);
  results->SetWithoutPathExpansion("mediaFileSystems", std::move(list));
  results->SetKey("selectedFileSystemIndex", base::Value(index));
  SetResult(std::move(results));
  SendResponse(true);
}

void MediaGalleriesAddUserSelectedFolderFunction::
    GetMediaFileSystemsForExtension(const MediaFileSystemsCallback& cb) {
  if (!render_frame_host()) {
    cb.Run(std::vector<MediaFileSystemInfo>());
    return;
  }
  MediaFileSystemRegistry* registry = media_file_system_registry();
  DCHECK(registry->GetPreferences(GetProfile())->IsInitialized());
  registry->GetMediaFileSystemsForExtension(GetSenderWebContents(), extension(),
                                            cb);
}

///////////////////////////////////////////////////////////////////////////////
//                 MediaGalleriesGetMetadataFunction                         //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesGetMetadataFunction::~MediaGalleriesGetMetadataFunction() {}

bool MediaGalleriesGetMetadataFunction::RunAsync() {
  ::media_galleries::UsageCount(::media_galleries::GET_METADATA);
  std::string blob_uuid;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &blob_uuid));

  const base::Value* options_value = NULL;
  if (!args_->Get(1, &options_value))
    return false;
  std::unique_ptr<MediaGalleries::MediaMetadataOptions> options =
      MediaGalleries::MediaMetadataOptions::FromValue(*options_value);
  if (!options)
    return false;

  return Setup(GetProfile(), &error_,
               base::Bind(&MediaGalleriesGetMetadataFunction::OnPreferencesInit,
                          this, options->metadata_type, blob_uuid));
}

void MediaGalleriesGetMetadataFunction::OnPreferencesInit(
    MediaGalleries::GetMetadataType metadata_type,
    const std::string& blob_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BlobReader::Read(GetProfile(), blob_uuid,
                   base::Bind(&MediaGalleriesGetMetadataFunction::GetMetadata,
                              this, metadata_type, blob_uuid),
                   0, net::kMaxBytesToSniff);
}

void MediaGalleriesGetMetadataFunction::GetMetadata(
    MediaGalleries::GetMetadataType metadata_type,
    const std::string& blob_uuid,
    std::unique_ptr<std::string> blob_header,
    int64_t total_blob_length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string mime_type;
  bool mime_type_sniffed = net::SniffMimeTypeFromLocalData(
      blob_header->c_str(), blob_header->size(), &mime_type);

  if (!mime_type_sniffed) {
    SendResponse(false);
    return;
  }

  if (metadata_type == MediaGalleries::GET_METADATA_TYPE_MIMETYPEONLY) {
    MediaGalleries::MediaMetadata metadata;
    metadata.mime_type = mime_type;

    std::unique_ptr<base::DictionaryValue> result_dictionary(
        new base::DictionaryValue);
    result_dictionary->Set(kMetadataKey, metadata.ToValue());
    SetResult(std::move(result_dictionary));
    SendResponse(true);
    return;
  }

  // We get attached images by default. GET_METADATA_TYPE_NONE is the default
  // value if the caller doesn't specify the metadata type.
  bool get_attached_images =
      metadata_type == MediaGalleries::GET_METADATA_TYPE_ALL ||
      metadata_type == MediaGalleries::GET_METADATA_TYPE_NONE;

  auto media_data_source_factory =
      std::make_unique<BlobDataSourceFactory>(GetProfile(), blob_uuid);
  auto parser = std::make_unique<SafeMediaMetadataParser>(
      total_blob_length, mime_type, get_attached_images,
      std::move(media_data_source_factory));
  SafeMediaMetadataParser* parser_ptr = parser.get();
  parser_ptr->Start(
      base::BindOnce(
          &MediaGalleriesGetMetadataFunction::OnSafeMediaMetadataParserDone,
          this, std::move(parser)));
}

void MediaGalleriesGetMetadataFunction::OnSafeMediaMetadataParserDone(
    std::unique_ptr<SafeMediaMetadataParser> parser_keep_alive,
    bool parse_success,
    chrome::mojom::MediaMetadataPtr metadata,
    std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!parse_success) {
    SendResponse(false);
    return;
  }

  DCHECK(metadata);
  DCHECK(attached_images);

  std::unique_ptr<base::DictionaryValue> result_dictionary(
      new base::DictionaryValue);
  result_dictionary->Set(kMetadataKey,
                         SerializeMediaMetadata(std::move(metadata)));

  if (attached_images->empty()) {
    SetResult(std::move(result_dictionary));
    SendResponse(true);
    return;
  }

  result_dictionary->Set(kAttachedImagesBlobInfoKey,
                         std::make_unique<base::ListValue>());
  metadata::AttachedImage* first_image = &attached_images->front();
  content::BrowserContext::CreateMemoryBackedBlob(
      GetProfile(), base::as_bytes(base::make_span(first_image->data)), "",
      base::BindOnce(&MediaGalleriesGetMetadataFunction::ConstructNextBlob,
                     this, std::move(result_dictionary),
                     std::move(attached_images),
                     base::WrapUnique(new std::vector<std::string>)));
}

void MediaGalleriesGetMetadataFunction::ConstructNextBlob(
    std::unique_ptr<base::DictionaryValue> result_dictionary,
    std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images,
    std::unique_ptr<std::vector<std::string>> blob_uuids,
    std::unique_ptr<content::BlobHandle> current_blob) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(result_dictionary.get());
  DCHECK(attached_images.get());
  DCHECK(blob_uuids.get());
  DCHECK(current_blob.get());

  DCHECK(!attached_images->empty());
  DCHECK_LT(blob_uuids->size(), attached_images->size());

  // For the newly constructed Blob, store its image's metadata and Blob UUID.
  base::ListValue* attached_images_list = NULL;
  result_dictionary->GetList(kAttachedImagesBlobInfoKey, &attached_images_list);
  DCHECK(attached_images_list);
  DCHECK_LT(attached_images_list->GetSize(), attached_images->size());

  metadata::AttachedImage* current_image =
      &(*attached_images)[blob_uuids->size()];
  std::unique_ptr<base::DictionaryValue> attached_image(
      new base::DictionaryValue);
  attached_image->SetString(kBlobUUIDKey, current_blob->GetUUID());
  attached_image->SetString(kMediaGalleriesApiTypeKey, current_image->type);
  attached_image->SetInteger(
      kSizeKey, base::checked_cast<int>(current_image->data.size()));
  attached_images_list->Append(std::move(attached_image));

  blob_uuids->push_back(current_blob->GetUUID());

  if (!render_frame_host() || !render_frame_host()->GetProcess()) {
    SendResponse(false);
    return;
  }

  extensions::BlobHolder* holder =
      extensions::BlobHolder::FromRenderProcessHost(
          render_frame_host()->GetProcess());
  holder->HoldBlobReference(std::move(current_blob));

  // Construct the next Blob if necessary.
  if (blob_uuids->size() < attached_images->size()) {
    metadata::AttachedImage* next_image =
        &(*attached_images)[blob_uuids->size()];
    content::BrowserContext::CreateMemoryBackedBlob(
        GetProfile(), base::as_bytes(base::make_span(next_image->data)), "",
        base::BindOnce(&MediaGalleriesGetMetadataFunction::ConstructNextBlob,
                       this, std::move(result_dictionary),
                       std::move(attached_images), std::move(blob_uuids)));
    return;
  }

  // All Blobs have been constructed. The renderer will take ownership.
  SetResult(std::move(result_dictionary));
  SetTransferredBlobUUIDs(*blob_uuids);
  SendResponse(true);
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesAddGalleryWatchFunction                        //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesAddGalleryWatchFunction::
    ~MediaGalleriesAddGalleryWatchFunction() {}

bool MediaGalleriesAddGalleryWatchFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(GetProfile());
  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return false;

  std::unique_ptr<AddGalleryWatch::Params> params(
      AddGalleryWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  preferences->EnsureInitialized(
      base::Bind(&MediaGalleriesAddGalleryWatchFunction::OnPreferencesInit,
                 this, params->gallery_id));

  return true;
}

void MediaGalleriesAddGalleryWatchFunction::OnPreferencesInit(
    const std::string& pref_id) {
  base::FilePath gallery_file_path;
  MediaGalleryPrefId gallery_pref_id = kInvalidMediaGalleryPrefId;
  if (!GetGalleryFilePathAndId(pref_id, GetProfile(), extension(),
                               &gallery_file_path, &gallery_pref_id)) {
    api::media_galleries::AddGalleryWatchResult result;
    error_ = kInvalidGalleryIdMsg;
    result.gallery_id = kInvalidGalleryId;
    result.success = false;
    SetResult(result.ToValue());
    SendResponse(false);
    return;
  }

  gallery_watch_manager()->AddWatch(
      GetProfile(), extension(), gallery_pref_id,
      base::Bind(&MediaGalleriesAddGalleryWatchFunction::HandleResponse, this,
                 gallery_pref_id));
}

void MediaGalleriesAddGalleryWatchFunction::HandleResponse(
    MediaGalleryPrefId gallery_id,
    const std::string& error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If an app added a file watch without any event listeners on the
  // onGalleryChanged event, that's an error.
  MediaGalleriesEventRouter* api = MediaGalleriesEventRouter::Get(GetProfile());
  api::media_galleries::AddGalleryWatchResult result;
  result.gallery_id = base::NumberToString(gallery_id);

  if (!api->ExtensionHasGalleryChangeListener(extension()->id())) {
    result.success = false;
    SetResult(result.ToValue());
    error_ = kMissingEventListener;
    SendResponse(false);
    return;
  }

  result.success = error.empty();
  SetResult(result.ToValue());
  if (error.empty()) {
    SendResponse(true);
  } else {
    error_ = error.c_str();
    SendResponse(false);
  }
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesRemoveGalleryWatchFunction                     //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesRemoveGalleryWatchFunction::
    ~MediaGalleriesRemoveGalleryWatchFunction() {}

bool MediaGalleriesRemoveGalleryWatchFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return false;

  std::unique_ptr<RemoveGalleryWatch::Params> params(
      RemoveGalleryWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  preferences->EnsureInitialized(
      base::Bind(&MediaGalleriesRemoveGalleryWatchFunction::OnPreferencesInit,
                 this, params->gallery_id));
  return true;
}

void MediaGalleriesRemoveGalleryWatchFunction::OnPreferencesInit(
    const std::string& pref_id) {
  base::FilePath gallery_file_path;
  MediaGalleryPrefId gallery_pref_id = 0;
  if (!GetGalleryFilePathAndId(pref_id, GetProfile(), extension(),
                               &gallery_file_path, &gallery_pref_id)) {
    error_ = kInvalidGalleryIdMsg;
    SendResponse(false);
    return;
  }

  gallery_watch_manager()->RemoveWatch(GetProfile(), extension_id(),
                                       gallery_pref_id);
  SendResponse(true);
}

}  // namespace api
}  // namespace chrome_apps
