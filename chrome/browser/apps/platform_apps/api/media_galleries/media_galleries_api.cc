// Copyright 2012 The Chromium Authors
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

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/platform_apps/api/deprecation_features.h"
#include "chrome/browser/apps/platform_apps/api/media_galleries/blob_data_source_factory.h"
#include "chrome/browser/apps/platform_apps/api/media_galleries/media_galleries_api_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media_galleries/gallery_watch_manager.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
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
#include "extensions/browser/blob_reader.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/mime_sniffer.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

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

const char kInvalidGalleryId[] = "-1";

const char kDeprecatedError[] =
    "Media Galleries API is deprecated on this platform.";
const char kNoRenderFrameOrRenderProcessError[] =
    "No render frame or render process.";
const char kNoWebContentsError[] = "Could not find web contents.";

MediaFileSystemRegistry* media_file_system_registry() {
  return g_browser_process->media_file_system_registry();
}

GalleryWatchManager* gallery_watch_manager() {
  return media_file_system_registry()->gallery_watch_manager();
}

// Checks whether the MediaGalleries API is currently accessible (it may be
// disallowed even if an extension has the requisite permission). Then
// initializes the MediaGalleriesPreferences
bool Setup(Profile* profile, std::string* error, base::OnceClosure callback) {
  if (!ChromeSelectFilePolicy::FileSelectDialogsAllowed()) {
    *error =
        std::string(kDisallowedByPolicy) + prefs::kAllowFileSelectionDialogs;
    return false;
  }

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(profile);
  preferences->EnsureInitialized(std::move(callback));
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

std::optional<base::Value::List> ConstructFileSystemList(
    content::RenderFrameHost* rfh,
    const extensions::Extension* extension,
    const std::vector<MediaFileSystemInfo>& filesystems) {
  if (!rfh)
    return std::nullopt;

  MediaGalleriesPermission::CheckParam read_param(
      MediaGalleriesPermission::kReadPermission);
  const extensions::PermissionsData* permissions_data =
      extension->permissions_data();
  bool has_read_permission = permissions_data->CheckAPIPermissionWithParam(
      extensions::mojom::APIPermissionID::kMediaGalleries, &read_param);
  MediaGalleriesPermission::CheckParam copy_to_param(
      MediaGalleriesPermission::kCopyToPermission);
  bool has_copy_to_permission = permissions_data->CheckAPIPermissionWithParam(
      extensions::mojom::APIPermissionID::kMediaGalleries, &copy_to_param);
  MediaGalleriesPermission::CheckParam delete_param(
      MediaGalleriesPermission::kDeletePermission);
  bool has_delete_permission = permissions_data->CheckAPIPermissionWithParam(
      extensions::mojom::APIPermissionID::kMediaGalleries, &delete_param);

  const int child_id = rfh->GetProcess()->GetID();
  base::Value::List list;
  for (const auto& filesystem : filesystems) {
    base::Value::Dict file_system_dict_value;

    // Send the file system id so the renderer can create a valid FileSystem
    // object.
    file_system_dict_value.Set("fsid", filesystem.fsid);

    file_system_dict_value.Set(kNameKey, filesystem.name);
    file_system_dict_value.Set(kGalleryIdKey,
                               base::NumberToString(filesystem.pref_id));
    if (!filesystem.transient_device_id.empty()) {
      file_system_dict_value.Set(kDeviceIdKey, filesystem.transient_device_id);
    }
    file_system_dict_value.Set(kIsRemovableKey, filesystem.removable);
    file_system_dict_value.Set(kIsMediaDeviceKey, filesystem.media_device);
    file_system_dict_value.Set(kIsAvailableKey, true);

    list.Append(std::move(file_system_dict_value));

    if (filesystem.path.empty())
      continue;

    if (has_read_permission) {
      content::ChildProcessSecurityPolicy* policy =
          content::ChildProcessSecurityPolicy::GetInstance();
      policy->GrantReadFile(child_id, filesystem.path);
      if (has_delete_permission) {
        policy->GrantDeleteFrom(child_id, filesystem.path);
        if (has_copy_to_permission) {
          policy->GrantCopyInto(child_id, filesystem.path);
        }
      }
    }
  }

  return list;
}

class SelectDirectoryDialog : public ui::SelectFileDialog::Listener,
                              public base::RefCounted<SelectDirectoryDialog> {
 public:
  // Selected file path, or an empty path if the user canceled.
  using Callback = base::RepeatingCallback<void(const base::FilePath&)>;

  SelectDirectoryDialog(WebContents* web_contents, Callback callback)
      : web_contents_(web_contents), callback_(std::move(callback)) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  }
  SelectDirectoryDialog(const SelectDirectoryDialog&) = delete;
  SelectDirectoryDialog& operator=(const SelectDirectoryDialog&) = delete;

  void Show(const base::FilePath& default_path) {
    AddRef();  // Balanced in the two reachable listener outcomes.
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_FOLDER,
        l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY_TITLE),
        default_path, nullptr, 0, base::FilePath::StringType(),
        platform_util::GetTopLevel(web_contents_->GetNativeView()));
  }

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override {
    callback_.Run(file.path());
    Release();  // Balanced in Show().
  }

  void FileSelectionCanceled() override {
    callback_.Run(base::FilePath());
    Release();  // Balanced in Show().
  }

 private:
  friend class base::RefCounted<SelectDirectoryDialog>;
  ~SelectDirectoryDialog() override {
    select_file_dialog_->ListenerDestroyed();
  }

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  raw_ptr<WebContents> web_contents_;
  Callback callback_;
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

MediaGalleriesEventRouter::~MediaGalleriesEventRouter() = default;

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
    base::Value::List event_args) {
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
  details.type = MediaGalleries::GalleryChangeType::kContentsChanged;
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
  details.type = MediaGalleries::GalleryChangeType::kWatchDropped;
  details.gallery_id = base::NumberToString(gallery_id);
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

ExtensionFunction::ResponseAction
MediaGalleriesGetMediaFileSystemsFunction::Run() {
  if (base::FeatureList::IsEnabled(features::kDeprecateMediaGalleriesApis))
    return RespondNow(Error(kDeprecatedError));

  std::optional<GetMediaFileSystems::Params> params(
      GetMediaFileSystems::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  MediaGalleries::GetMediaFileSystemsInteractivity interactive =
      MediaGalleries::GetMediaFileSystemsInteractivity::kNo;
  if (params->details &&
      params->details->interactive !=
          MediaGalleries::GetMediaFileSystemsInteractivity::kNone) {
    interactive = params->details->interactive;
  }

  std::string error;
  const bool result =
      Setup(Profile::FromBrowserContext(browser_context()), &error,
            base::BindOnce(
                &MediaGalleriesGetMediaFileSystemsFunction::OnPreferencesInit,
                this, interactive));
  if (!result)
    return RespondNow(Error(error));
  // Note: OnPreferencesInit might have been called already.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void MediaGalleriesGetMediaFileSystemsFunction::OnPreferencesInit(
    MediaGalleries::GetMediaFileSystemsInteractivity interactive) {
  switch (interactive) {
    case MediaGalleries::GetMediaFileSystemsInteractivity::kYes: {
      // The MediaFileSystemRegistry only updates preferences for extensions
      // that it knows are in use. Since this may be the first call to
      // chrome.getMediaFileSystems for this extension, call
      // GetMediaFileSystemsForExtension() here solely so that
      // MediaFileSystemRegistry will send preference changes.
      GetMediaFileSystemsForExtension(base::BindOnce(
          &MediaGalleriesGetMediaFileSystemsFunction::AlwaysShowDialog, this));
      return;
    }
    case MediaGalleries::GetMediaFileSystemsInteractivity::kIfNeeded: {
      GetMediaFileSystemsForExtension(base::BindOnce(
          &MediaGalleriesGetMediaFileSystemsFunction::ShowDialogIfNoGalleries,
          this));
      return;
    }
    case MediaGalleries::GetMediaFileSystemsInteractivity::kNo:
      GetAndReturnGalleries();
      return;
    case MediaGalleries::GetMediaFileSystemsInteractivity::kNone:
      NOTREACHED_IN_MIGRATION();
  }
  Respond(Error("Error initializing Media Galleries preferences."));
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
  GetMediaFileSystemsForExtension(base::BindOnce(
      &MediaGalleriesGetMediaFileSystemsFunction::ReturnGalleries, this));
}

void MediaGalleriesGetMediaFileSystemsFunction::ReturnGalleries(
    const std::vector<MediaFileSystemInfo>& filesystems) {
  std::optional<base::Value::List> list =
      ConstructFileSystemList(render_frame_host(), extension(), filesystems);
  if (!list) {
    Respond(Error("Error returning Media Galleries filesystems."));
    return;
  }

  // The custom JS binding will use this list to create DOMFileSystem objects.
  Respond(WithArguments(std::move(*list)));
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialog() {
  WebContents* contents = GetWebContentsForPrompt(
      GetSenderWebContents(), browser_context(), extension()->id());
  if (!contents) {
    Respond(Error(kNoWebContentsError));
    return;
  }

  // Controller will delete itself.
  base::OnceClosure cb = base::BindOnce(
      &MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries, this);
  new MediaGalleriesPermissionController(contents, *extension(), std::move(cb));
}

void MediaGalleriesGetMediaFileSystemsFunction::GetMediaFileSystemsForExtension(
    MediaFileSystemsCallback cb) {
  if (!render_frame_host()) {
    std::move(cb).Run(std::vector<MediaFileSystemInfo>());
    return;
  }
  MediaFileSystemRegistry* registry = media_file_system_registry();
  Profile* profile = Profile::FromBrowserContext(browser_context());
  DCHECK(registry->GetPreferences(profile)->IsInitialized());
  registry->GetMediaFileSystemsForExtension(GetSenderWebContents(), extension(),
                                            std::move(cb));
}

///////////////////////////////////////////////////////////////////////////////
//               MediaGalleriesAddUserSelectedFolderFunction                 //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesAddUserSelectedFolderFunction::
    ~MediaGalleriesAddUserSelectedFolderFunction() {}

ExtensionFunction::ResponseAction
MediaGalleriesAddUserSelectedFolderFunction::Run() {
  if (base::FeatureList::IsEnabled(features::kDeprecateMediaGalleriesApis))
    return RespondNow(Error(kDeprecatedError));

  std::string error;
  const bool result =
      Setup(Profile::FromBrowserContext(browser_context()), &error,
            base::BindOnce(
                &MediaGalleriesAddUserSelectedFolderFunction::OnPreferencesInit,
                this));
  if (!result)
    return RespondNow(Error(error));
  // Note: OnPreferencesInit might have been called already.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void MediaGalleriesAddUserSelectedFolderFunction::OnPreferencesInit() {
  const std::string& app_id = extension()->id();
  WebContents* contents = GetWebContentsForPrompt(GetSenderWebContents(),
                                                  browser_context(), app_id);
  if (!contents) {
    Respond(Error(kNoWebContentsError));
    return;
  }

  if (!user_gesture()) {
    OnDirectorySelected(base::FilePath());
    return;
  }

  base::FilePath last_used_path =
      extensions::file_system_api::GetLastChooseEntryDirectory(
          extensions::ExtensionPrefs::Get(browser_context()), app_id);
  SelectDirectoryDialog::Callback callback = base::BindRepeating(
      &MediaGalleriesAddUserSelectedFolderFunction::OnDirectorySelected, this);
  auto select_directory_dialog = base::MakeRefCounted<SelectDirectoryDialog>(
      contents, std::move(callback));
  select_directory_dialog->Show(last_used_path);
}

void MediaGalleriesAddUserSelectedFolderFunction::OnDirectorySelected(
    const base::FilePath& selected_directory) {
  if (selected_directory.empty()) {
    // User cancelled case.
    GetMediaFileSystemsForExtension(base::BindOnce(
        &MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId,
        this, kInvalidMediaGalleryPrefId));
    return;
  }

  extensions::file_system_api::SetLastChooseEntryDirectory(
      extensions::ExtensionPrefs::Get(browser_context()), extension()->id(),
      selected_directory);

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(
          Profile::FromBrowserContext(browser_context()));
  MediaGalleryPrefId pref_id = preferences->AddGalleryByPath(
      selected_directory, MediaGalleryPrefInfo::kUserAdded);
  preferences->SetGalleryPermissionForExtension(*extension(), pref_id, true);

  GetMediaFileSystemsForExtension(base::BindOnce(
      &MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId, this,
      pref_id));
}

void MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId(
    MediaGalleryPrefId pref_id,
    const std::vector<MediaFileSystemInfo>& filesystems) {
  std::optional<base::Value::List> list =
      ConstructFileSystemList(render_frame_host(), extension(), filesystems);
  if (!list) {
    Respond(Error("Error returning Media Galleries filesystems."));
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
  base::Value::Dict results;
  results.Set("mediaFileSystems", std::move(*list));
  results.Set("selectedFileSystemIndex", base::Value(index));
  Respond(WithArguments(std::move(results)));
}

void MediaGalleriesAddUserSelectedFolderFunction::
    GetMediaFileSystemsForExtension(MediaFileSystemsCallback cb) {
  if (!render_frame_host()) {
    std::move(cb).Run(std::vector<MediaFileSystemInfo>());
    return;
  }
  MediaFileSystemRegistry* registry = media_file_system_registry();
  DCHECK(
      registry->GetPreferences(Profile::FromBrowserContext(browser_context()))
          ->IsInitialized());
  registry->GetMediaFileSystemsForExtension(GetSenderWebContents(), extension(),
                                            std::move(cb));
}

///////////////////////////////////////////////////////////////////////////////
//                 MediaGalleriesGetMetadataFunction                         //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesGetMetadataFunction::~MediaGalleriesGetMetadataFunction() {}

ExtensionFunction::ResponseAction MediaGalleriesGetMetadataFunction::Run() {
  if (base::FeatureList::IsEnabled(features::kDeprecateMediaGalleriesApis))
    return RespondNow(Error(kDeprecatedError));

  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& blob_uuid = args()[0].GetString();

  if (args().size() < 2)
    return RespondNow(Error("options parameter not specified."));

  std::optional<MediaGalleries::MediaMetadataOptions> options =
      MediaGalleries::MediaMetadataOptions::FromValue(args()[1]);
  if (!options)
    return RespondNow(Error("Invalid value for options parameter."));

  std::string error;
  const bool result = Setup(
      Profile::FromBrowserContext(browser_context()), &error,
      base::BindOnce(&MediaGalleriesGetMetadataFunction::OnPreferencesInit,
                     this, options->metadata_type, blob_uuid));
  if (!result)
    return RespondNow(Error(error));
  // Note: OnPreferencesInit might have been called already.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void MediaGalleriesGetMetadataFunction::OnPreferencesInit(
    MediaGalleries::GetMetadataType metadata_type,
    const std::string& blob_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BlobReader::Read(
      browser_context()->GetBlobRemote(blob_uuid),
      base::BindOnce(&MediaGalleriesGetMetadataFunction::GetMetadata, this,
                     metadata_type, blob_uuid),
      0, net::kMaxBytesToSniff);
}

void MediaGalleriesGetMetadataFunction::GetMetadata(
    MediaGalleries::GetMetadataType metadata_type,
    const std::string& blob_uuid,
    std::string blob_header,
    int64_t total_blob_length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string mime_type;
  bool mime_type_sniffed =
      net::SniffMimeTypeFromLocalData(blob_header, &mime_type);

  if (!mime_type_sniffed) {
    Respond(Error("Could not determine MIME type."));
    return;
  }

  if (metadata_type == MediaGalleries::GetMetadataType::kMimeTypeOnly) {
    MediaGalleries::MediaMetadata metadata;
    metadata.mime_type = mime_type;

    base::Value::Dict result_dictionary;
    result_dictionary.Set(kMetadataKey, metadata.ToValue());
    Respond(WithArguments(std::move(result_dictionary)));
    return;
  }

  // We get attached images by default. GET_METADATA_TYPE_NONE is the default
  // value if the caller doesn't specify the metadata type.
  bool get_attached_images =
      metadata_type == MediaGalleries::GetMetadataType::kAll ||
      metadata_type == MediaGalleries::GetMetadataType::kNone;

  auto media_data_source_factory =
      std::make_unique<BlobDataSourceFactory>(browser_context(), blob_uuid);
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
    Respond(Error("Could not parse media metadata."));
    return;
  }

  DCHECK(metadata);
  DCHECK(attached_images);

  base::Value::Dict result_dictionary;
  result_dictionary.Set(kMetadataKey,
                        SerializeMediaMetadata(std::move(metadata)));

  if (attached_images->empty()) {
    Respond(WithArguments(std::move(result_dictionary)));
    return;
  }

  metadata::AttachedImage* first_image = &attached_images->front();
  browser_context()->CreateMemoryBackedBlob(
      base::as_bytes(base::make_span(first_image->data)), first_image->type,
      base::BindOnce(&MediaGalleriesGetMetadataFunction::ConstructNextBlob,
                     this, std::move(result_dictionary),
                     std::move(attached_images),
                     std::vector<blink::mojom::SerializedBlobPtr>()));
}

void MediaGalleriesGetMetadataFunction::ConstructNextBlob(
    base::Value::Dict result_dictionary,
    std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images,
    std::vector<blink::mojom::SerializedBlobPtr> blobs,
    std::unique_ptr<content::BlobHandle> current_blob) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(attached_images.get());
  DCHECK(current_blob.get());

  DCHECK(!attached_images->empty());
  DCHECK_LT(blobs.size(), attached_images->size());

  blobs.push_back(current_blob->Serialize());

  if (!render_frame_host() || !render_frame_host()->GetProcess()) {
    Respond(Error(kNoRenderFrameOrRenderProcessError));
    return;
  }

  // Construct the next Blob if necessary.
  if (blobs.size() < attached_images->size()) {
    metadata::AttachedImage* next_image = &(*attached_images)[blobs.size()];
    browser_context()->CreateMemoryBackedBlob(
        base::as_bytes(base::make_span(next_image->data)), next_image->type,
        base::BindOnce(&MediaGalleriesGetMetadataFunction::ConstructNextBlob,
                       this, std::move(result_dictionary),
                       std::move(attached_images), std::move(blobs)));
    return;
  }

  // All Blobs have been constructed. The renderer will take ownership.
  SetTransferredBlobs(std::move(blobs));
  Respond(WithArguments(std::move(result_dictionary)));
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesAddGalleryWatchFunction                        //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesAddGalleryWatchFunction::
    ~MediaGalleriesAddGalleryWatchFunction() {}

ExtensionFunction::ResponseAction MediaGalleriesAddGalleryWatchFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::FeatureList::IsEnabled(features::kDeprecateMediaGalleriesApis))
    return RespondNow(Error(kDeprecatedError));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  DCHECK(profile);

  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return RespondNow(Error(kNoRenderFrameOrRenderProcessError));

  std::optional<AddGalleryWatch::Params> params(
      AddGalleryWatch::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  preferences->EnsureInitialized(
      base::BindOnce(&MediaGalleriesAddGalleryWatchFunction::OnPreferencesInit,
                     this, params->gallery_id));
  // Note: OnPreferencesInit might have been called already.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void MediaGalleriesAddGalleryWatchFunction::OnPreferencesInit(
    const std::string& pref_id) {
  base::FilePath gallery_file_path;
  MediaGalleryPrefId gallery_pref_id = kInvalidMediaGalleryPrefId;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!GetGalleryFilePathAndId(pref_id, profile, extension(),
                               &gallery_file_path, &gallery_pref_id)) {
    api::media_galleries::AddGalleryWatchResult result;
    result.gallery_id = kInvalidGalleryId;
    result.success = false;
    Respond(ErrorWithArguments(AddGalleryWatch::Results::Create(result),
                               kInvalidGalleryIdMsg));
    return;
  }

  gallery_watch_manager()->AddWatch(
      profile, extension(), gallery_pref_id,
      base::BindOnce(&MediaGalleriesAddGalleryWatchFunction::HandleResponse,
                     this, gallery_pref_id));
}

void MediaGalleriesAddGalleryWatchFunction::HandleResponse(
    MediaGalleryPrefId gallery_id,
    const std::string& error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If an app added a file watch without any event listeners on the
  // onGalleryChanged event, that's an error.
  MediaGalleriesEventRouter* api =
      MediaGalleriesEventRouter::Get(browser_context());
  api::media_galleries::AddGalleryWatchResult result;
  result.gallery_id = base::NumberToString(gallery_id);

  if (!api->ExtensionHasGalleryChangeListener(extension()->id())) {
    result.success = false;
    Respond(ErrorWithArguments(AddGalleryWatch::Results::Create(result),
                               kMissingEventListener));
    return;
  }

  result.success = error.empty();
  Respond(error.empty() ? WithArguments(result.ToValue())
                        : ErrorWithArguments(
                              AddGalleryWatch::Results::Create(result), error));
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesRemoveGalleryWatchFunction                     //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesRemoveGalleryWatchFunction::
    ~MediaGalleriesRemoveGalleryWatchFunction() {}

ExtensionFunction::ResponseAction
MediaGalleriesRemoveGalleryWatchFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::FeatureList::IsEnabled(features::kDeprecateMediaGalleriesApis))
    return RespondNow(Error(kDeprecatedError));

  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return RespondNow(Error(kNoRenderFrameOrRenderProcessError));

  std::optional<RemoveGalleryWatch::Params> params(
      RemoveGalleryWatch::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          Profile::FromBrowserContext(browser_context()));
  preferences->EnsureInitialized(base::BindOnce(
      &MediaGalleriesRemoveGalleryWatchFunction::OnPreferencesInit, this,
      params->gallery_id));
  // Note: OnPreferencesInit might have been called already.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void MediaGalleriesRemoveGalleryWatchFunction::OnPreferencesInit(
    const std::string& pref_id) {
  base::FilePath gallery_file_path;
  MediaGalleryPrefId gallery_pref_id = 0;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!GetGalleryFilePathAndId(pref_id, profile, extension(),
                               &gallery_file_path, &gallery_pref_id)) {
    Respond(Error(kInvalidGalleryIdMsg));
    return;
  }

  gallery_watch_manager()->RemoveWatch(profile, extension_id(),
                                       gallery_pref_id);
  Respond(NoArguments());
}

}  // namespace api
}  // namespace chrome_apps
