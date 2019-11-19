// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_permission_controller.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/media_galleries/media_gallery_context_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/apps/platform_apps/media_galleries_permission.h"
#include "chrome/grit/generated_resources.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/text/bytes_formatting.h"

using extensions::APIPermission;
using extensions::Extension;
using storage_monitor::StorageInfo;
using storage_monitor::StorageMonitor;

namespace {

// Comparator for sorting gallery entries. Sort Removable entries above
// non-removable ones. Within those two groups, sort on media counts
// if populated, otherwise on paths.
bool GalleriesVectorComparator(
    const MediaGalleriesDialogController::Entry& a,
    const MediaGalleriesDialogController::Entry& b) {
  if (StorageInfo::IsRemovableDevice(a.pref_info.device_id) !=
      StorageInfo::IsRemovableDevice(b.pref_info.device_id)) {
    return StorageInfo::IsRemovableDevice(a.pref_info.device_id);
  }
  int a_media_count = a.pref_info.audio_count + a.pref_info.image_count +
      a.pref_info.video_count;
  int b_media_count = b.pref_info.audio_count + b.pref_info.image_count +
      b.pref_info.video_count;
  if (a_media_count != b_media_count)
    return a_media_count > b_media_count;
  return a.pref_info.AbsolutePath() < b.pref_info.AbsolutePath();
}

}  // namespace

MediaGalleriesPermissionController::MediaGalleriesPermissionController(
    content::WebContents* web_contents,
    const Extension& extension,
    const base::Closure& on_finish)
      : web_contents_(web_contents),
        extension_(&extension),
        on_finish_(on_finish),
        preferences_(
            g_browser_process->media_file_system_registry()->GetPreferences(
                GetProfile())),
        create_dialog_callback_(base::Bind(&MediaGalleriesDialog::Create)) {
  // Passing unretained pointer is safe, since the dialog controller
  // is self-deleting, and so won't be deleted until it can be shown
  // and then closed.
  preferences_->EnsureInitialized(
      base::Bind(&MediaGalleriesPermissionController::OnPreferencesInitialized,
                 base::Unretained(this)));

  // Unretained is safe because |this| owns |context_menu_|.
  context_menu_.reset(
      new MediaGalleryContextMenu(
          base::Bind(&MediaGalleriesPermissionController::DidForgetEntry,
                     base::Unretained(this))));
}

void MediaGalleriesPermissionController::OnPreferencesInitialized() {
  DCHECK(StorageMonitor::GetInstance());
  StorageMonitor::GetInstance()->AddObserver(this);

  // |preferences_| may be NULL in tests.
  if (preferences_) {
    preferences_->AddGalleryChangeObserver(this);
    InitializePermissions();
  }

  dialog_.reset(create_dialog_callback_.Run(this));
}

MediaGalleriesPermissionController::MediaGalleriesPermissionController(
    const extensions::Extension& extension,
    MediaGalleriesPreferences* preferences,
    const CreateDialogCallback& create_dialog_callback,
    const base::Closure& on_finish)
    : web_contents_(NULL),
      extension_(&extension),
      on_finish_(on_finish),
      preferences_(preferences),
      create_dialog_callback_(create_dialog_callback) {
  OnPreferencesInitialized();
}

MediaGalleriesPermissionController::~MediaGalleriesPermissionController() {
  DCHECK(StorageMonitor::GetInstance());
  StorageMonitor::GetInstance()->RemoveObserver(this);

  // |preferences_| may be NULL in tests.
  if (preferences_)
    preferences_->RemoveGalleryChangeObserver(this);

  if (select_folder_dialog_.get())
    select_folder_dialog_->ListenerDestroyed();
}

base::string16 MediaGalleriesPermissionController::GetHeader() const {
  return l10n_util::GetStringFUTF16(IDS_MEDIA_GALLERIES_DIALOG_HEADER,
                                    base::UTF8ToUTF16(extension_->name()));
}

base::string16 MediaGalleriesPermissionController::GetSubtext() const {
  chrome_apps::MediaGalleriesPermission::CheckParam copy_to_param(
      chrome_apps::MediaGalleriesPermission::kCopyToPermission);
  chrome_apps::MediaGalleriesPermission::CheckParam delete_param(
      chrome_apps::MediaGalleriesPermission::kDeletePermission);
  const extensions::PermissionsData* permission_data =
      extension_->permissions_data();
  bool has_copy_to_permission = permission_data->CheckAPIPermissionWithParam(
      APIPermission::kMediaGalleries, &copy_to_param);
  bool has_delete_permission = permission_data->CheckAPIPermissionWithParam(
      APIPermission::kMediaGalleries, &delete_param);

  int id;
  if (has_copy_to_permission)
    id = IDS_MEDIA_GALLERIES_DIALOG_SUBTEXT_READ_WRITE;
  else if (has_delete_permission)
    id = IDS_MEDIA_GALLERIES_DIALOG_SUBTEXT_READ_DELETE;
  else
    id = IDS_MEDIA_GALLERIES_DIALOG_SUBTEXT_READ_ONLY;

  return l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension_->name()));
}

bool MediaGalleriesPermissionController::IsAcceptAllowed() const {
  if (!toggled_galleries_.empty() || !forgotten_galleries_.empty())
    return true;

  for (auto iter = new_galleries_.begin(); iter != new_galleries_.end();
       ++iter) {
    if (iter->second.selected)
      return true;
  }

  return false;
}

std::vector<base::string16>
MediaGalleriesPermissionController::GetSectionHeaders() const {
  std::vector<base::string16> result;
  result.push_back(base::string16());  // First section has no header.
  result.push_back(
      l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_PERMISSION_SUGGESTIONS));
  return result;
}

// Note: sorts by display criterion: GalleriesVectorComparator.
MediaGalleriesDialogController::Entries
MediaGalleriesPermissionController::GetSectionEntries(size_t index) const {
  DCHECK_GT(2U, index);  // This dialog only has two sections.

  bool existing = !index;
  MediaGalleriesDialogController::Entries result;
  for (auto iter = known_galleries_.begin(); iter != known_galleries_.end();
       ++iter) {
    MediaGalleryPrefId pref_id = GetPrefId(iter->first);
    if (!base::Contains(forgotten_galleries_, iter->first) &&
        existing == base::Contains(pref_permitted_galleries_, pref_id)) {
      result.push_back(iter->second);
    }
  }
  if (existing) {
    for (auto iter = new_galleries_.begin(); iter != new_galleries_.end();
         ++iter) {
      result.push_back(iter->second);
    }
  }

  std::sort(result.begin(), result.end(), GalleriesVectorComparator);
  return result;
}

base::string16
MediaGalleriesPermissionController::GetAuxiliaryButtonText() const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY);
}

// This is the 'Add Folder' button.
void MediaGalleriesPermissionController::DidClickAuxiliaryButton() {
  base::FilePath default_path =
      extensions::file_system_api::GetLastChooseEntryDirectory(
          extensions::ExtensionPrefs::Get(GetProfile()), extension_->id());
  if (default_path.empty())
    base::PathService::Get(base::DIR_USER_DESKTOP, &default_path);
  select_folder_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(nullptr));
  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY_TITLE),
      default_path,
      NULL,
      0,
      base::FilePath::StringType(),
      web_contents_->GetTopLevelNativeWindow(),
      NULL);
}

void MediaGalleriesPermissionController::DidToggleEntry(
    GalleryDialogId gallery_id, bool selected) {
  // Check known galleries.
  auto iter = known_galleries_.find(gallery_id);
  if (iter != known_galleries_.end()) {
    if (iter->second.selected == selected)
      return;

    iter->second.selected = selected;
    toggled_galleries_[gallery_id] = selected;
    return;
  }

  iter = new_galleries_.find(gallery_id);
  if (iter != new_galleries_.end())
    iter->second.selected = selected;

  // Don't sort -- the dialog is open, and we don't want to adjust any
  // positions for future updates to the dialog contents until they are
  // redrawn.
}

void MediaGalleriesPermissionController::DidForgetEntry(
    GalleryDialogId gallery_id) {
  media_galleries::UsageCount(media_galleries::DIALOG_FORGET_GALLERY);
  if (!new_galleries_.erase(gallery_id)) {
    DCHECK(base::Contains(known_galleries_, gallery_id));
    forgotten_galleries_.insert(gallery_id);
  }
  dialog_->UpdateGalleries();
}

base::string16 MediaGalleriesPermissionController::GetAcceptButtonText() const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_CONFIRM);
}

void MediaGalleriesPermissionController::DialogFinished(bool accepted) {
  // The dialog has finished, so there is no need to watch for more updates
  // from |preferences_|.
  // |preferences_| may be NULL in tests.
  if (preferences_)
    preferences_->RemoveGalleryChangeObserver(this);

  if (accepted)
    SavePermissions();

  on_finish_.Run();

  delete this;
}

content::WebContents* MediaGalleriesPermissionController::WebContents() {
  return web_contents_;
}

void MediaGalleriesPermissionController::FileSelected(
    const base::FilePath& path,
    int /*index*/,
    void* /*params*/) {
  // |web_contents_| is NULL in tests.
  if (web_contents_) {
    extensions::file_system_api::SetLastChooseEntryDirectory(
          extensions::ExtensionPrefs::Get(GetProfile()),
          extension_->id(),
          path);
  }

  // Try to find it in the prefs.
  MediaGalleryPrefInfo gallery;
  DCHECK(preferences_);
  bool gallery_exists = preferences_->LookUpGalleryByPath(path, &gallery);
  if (gallery_exists && !gallery.IsBlackListedType()) {
    // The prefs are in sync with |known_galleries_|, so it should exist in
    // |known_galleries_| as well. User selecting a known gallery effectively
    // just sets the gallery to permitted.
    GalleryDialogId gallery_id = GetDialogId(gallery.pref_id);
    auto iter = known_galleries_.find(gallery_id);
    DCHECK(iter != known_galleries_.end());
    iter->second.selected = true;
    forgotten_galleries_.erase(gallery_id);
    dialog_->UpdateGalleries();
    return;
  }

  // Try to find it in |new_galleries_| (user added same folder twice).
  for (auto iter = new_galleries_.begin(); iter != new_galleries_.end();
       ++iter) {
    if (iter->second.pref_info.path == gallery.path &&
        iter->second.pref_info.device_id == gallery.device_id) {
      iter->second.selected = true;
      dialog_->UpdateGalleries();
      return;
    }
  }

  // Lastly, if not found, add a new gallery to |new_galleries_|.
  // prefId == kInvalidMediaGalleryPrefId for completely new galleries.
  // The old prefId is retained for blacklisted galleries.
  gallery.pref_id = GetDialogId(gallery.pref_id);
  new_galleries_[gallery.pref_id] = Entry(gallery, true);
  dialog_->UpdateGalleries();
}

void MediaGalleriesPermissionController::OnRemovableStorageAttached(
    const StorageInfo& info) {
  UpdateGalleriesOnDeviceEvent(info.device_id());
}

void MediaGalleriesPermissionController::OnRemovableStorageDetached(
    const StorageInfo& info) {
  UpdateGalleriesOnDeviceEvent(info.device_id());
}

void MediaGalleriesPermissionController::OnPermissionAdded(
    MediaGalleriesPreferences* /* prefs */,
    const std::string& extension_id,
    MediaGalleryPrefId /* pref_id */) {
  if (extension_id != extension_->id())
    return;
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesPermissionController::OnPermissionRemoved(
    MediaGalleriesPreferences* /* prefs */,
    const std::string& extension_id,
    MediaGalleryPrefId /* pref_id */) {
  if (extension_id != extension_->id())
    return;
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesPermissionController::OnGalleryAdded(
    MediaGalleriesPreferences* /* prefs */,
    MediaGalleryPrefId /* pref_id */) {
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesPermissionController::OnGalleryRemoved(
    MediaGalleriesPreferences* /* prefs */,
    MediaGalleryPrefId /* pref_id */) {
  UpdateGalleriesOnPreferencesEvent();
}

void MediaGalleriesPermissionController::OnGalleryInfoUpdated(
    MediaGalleriesPreferences* prefs,
    MediaGalleryPrefId pref_id) {
  DCHECK(preferences_);
  const MediaGalleriesPrefInfoMap& pref_galleries =
      preferences_->known_galleries();
  auto pref_it = pref_galleries.find(pref_id);
  if (pref_it == pref_galleries.end())
    return;
  const MediaGalleryPrefInfo& gallery_info = pref_it->second;
  UpdateGalleriesOnDeviceEvent(gallery_info.device_id);
}

void MediaGalleriesPermissionController::InitializePermissions() {
  known_galleries_.clear();
  DCHECK(preferences_);
  const MediaGalleriesPrefInfoMap& galleries = preferences_->known_galleries();
  for (auto iter = galleries.begin(); iter != galleries.end(); ++iter) {
    const MediaGalleryPrefInfo& gallery = iter->second;
    if (gallery.IsBlackListedType())
      continue;

    GalleryDialogId gallery_id = GetDialogId(gallery.pref_id);
    known_galleries_[gallery_id] = Entry(gallery, false);
    known_galleries_[gallery_id].pref_info.pref_id = gallery_id;
  }

  pref_permitted_galleries_ = preferences_->GalleriesForExtension(*extension_);

  for (auto iter = pref_permitted_galleries_.begin();
       iter != pref_permitted_galleries_.end(); ++iter) {
    GalleryDialogId gallery_id = GetDialogId(*iter);
    DCHECK(base::Contains(known_galleries_, gallery_id));
    known_galleries_[gallery_id].selected = true;
  }

  // Preserve state of toggled galleries.
  for (ToggledGalleryMap::const_iterator iter = toggled_galleries_.begin();
       iter != toggled_galleries_.end();
       ++iter) {
    known_galleries_[iter->first].selected = iter->second;
  }
}

void MediaGalleriesPermissionController::SavePermissions() {
  DCHECK(preferences_);
  media_galleries::UsageCount(media_galleries::SAVE_DIALOG);
  for (GalleryPermissionsMap::const_iterator iter = known_galleries_.begin();
       iter != known_galleries_.end(); ++iter) {
    MediaGalleryPrefId pref_id = GetPrefId(iter->first);
    if (base::Contains(forgotten_galleries_, iter->first)) {
      preferences_->ForgetGalleryById(pref_id);
    } else {
      bool changed = preferences_->SetGalleryPermissionForExtension(
          *extension_, pref_id, iter->second.selected);
      if (changed) {
        if (iter->second.selected) {
          media_galleries::UsageCount(
              media_galleries::DIALOG_PERMISSION_ADDED);
        } else {
          media_galleries::UsageCount(
              media_galleries::DIALOG_PERMISSION_REMOVED);
        }
      }
    }
  }

  for (GalleryPermissionsMap::const_iterator iter = new_galleries_.begin();
       iter != new_galleries_.end(); ++iter) {
    media_galleries::UsageCount(media_galleries::DIALOG_GALLERY_ADDED);
    // If the user added a gallery then unchecked it, forget about it.
    if (!iter->second.selected)
      continue;

    const MediaGalleryPrefInfo& gallery = iter->second.pref_info;
    MediaGalleryPrefId id = preferences_->AddGallery(
        gallery.device_id, gallery.path, MediaGalleryPrefInfo::kUserAdded,
        gallery.volume_label, gallery.vendor_name, gallery.model_name,
        gallery.total_size_in_bytes, gallery.last_attach_time, 0, 0, 0);
    preferences_->SetGalleryPermissionForExtension(*extension_, id, true);
  }
}

void MediaGalleriesPermissionController::UpdateGalleriesOnPreferencesEvent() {
  // Merge in the permissions from |preferences_|. Afterwards,
  // |known_galleries_| may contain galleries that no longer belong there,
  // but the code below will put |known_galleries_| back in a consistent state.
  InitializePermissions();

  std::set<GalleryDialogId> new_galleries_to_remove;
  // Look for duplicate entries in |new_galleries_| in case one was added
  // in another dialog.
  for (auto it = known_galleries_.begin(); it != known_galleries_.end(); ++it) {
    Entry& gallery = it->second;
    for (auto new_it = new_galleries_.begin(); new_it != new_galleries_.end();
         ++new_it) {
      if (new_it->second.pref_info.path == gallery.pref_info.path &&
          new_it->second.pref_info.device_id == gallery.pref_info.device_id) {
        // Found duplicate entry. Get the existing permission from it and then
        // remove it.
        gallery.selected = new_it->second.selected;
        new_galleries_to_remove.insert(new_it->first);
        break;
      }
    }
  }
  for (auto it = new_galleries_to_remove.begin();
       it != new_galleries_to_remove.end(); ++it) {
    new_galleries_.erase(*it);
  }

  dialog_->UpdateGalleries();
}

void MediaGalleriesPermissionController::UpdateGalleriesOnDeviceEvent(
    const std::string& device_id) {
  dialog_->UpdateGalleries();
}

ui::MenuModel* MediaGalleriesPermissionController::GetContextMenu(
    GalleryDialogId gallery_id) {
  context_menu_->set_pref_id(gallery_id);
  return context_menu_.get();
}

GalleryDialogId MediaGalleriesPermissionController::GetDialogId(
    MediaGalleryPrefId pref_id) {
  return id_map_.GetDialogId(pref_id);
}

MediaGalleryPrefId MediaGalleriesPermissionController::GetPrefId(
    GalleryDialogId id) const {
  return id_map_.GetPrefId(id);
}

Profile* MediaGalleriesPermissionController::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

MediaGalleriesPermissionController::DialogIdMap::DialogIdMap()
    : next_dialog_id_(1) {
  // Dialog id of 0 is invalid, so fill the slot.
  forward_mapping_.push_back(kInvalidMediaGalleryPrefId);
}

MediaGalleriesPermissionController::DialogIdMap::~DialogIdMap() {
}

GalleryDialogId
MediaGalleriesPermissionController::DialogIdMap::GetDialogId(
    MediaGalleryPrefId pref_id) {
  std::map<GalleryDialogId, MediaGalleryPrefId>::const_iterator it =
      back_map_.find(pref_id);
  if (it != back_map_.end())
    return it->second;

  GalleryDialogId result = next_dialog_id_++;
  DCHECK_EQ(result, forward_mapping_.size());
  forward_mapping_.push_back(pref_id);
  if (pref_id != kInvalidMediaGalleryPrefId)
    back_map_[pref_id] = result;
  return result;
}

MediaGalleryPrefId
MediaGalleriesPermissionController::DialogIdMap::GetPrefId(
    GalleryDialogId id) const {
  DCHECK_LT(id, next_dialog_id_);
  return forward_mapping_[id];
}

// MediaGalleries dialog -------------------------------------------------------

MediaGalleriesDialog::~MediaGalleriesDialog() {}
