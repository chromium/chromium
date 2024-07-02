// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PERMISSION_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PERMISSION_CONTROLLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace ui {
class MenuModel;
}

class MediaGalleriesDialogController;
class MediaGalleryContextMenu;
class Profile;

// Newly added galleries are not added to preferences until the dialog commits,
// so they do not have a pref id while the dialog is open; leading to
// complicated code in the dialogs. To solve this complication, the controller
// maps pref ids into a new space where it can also assign ids to new galleries.
// The new number space is only valid for the lifetime of the controller. To
// make it more clear where real pref ids are used and where the fake ids are
// used, the GalleryDialogId type is used where fake ids are needed.
typedef MediaGalleryPrefId GalleryDialogId;

class MediaGalleriesPermissionController
    : public MediaGalleriesDialogController,
      public ui::SelectFileDialog::Listener,
      public storage_monitor::RemovableStorageObserver,
      public MediaGalleriesPreferences::GalleryChangeObserver {
 public:
  // The constructor creates a dialog controller which owns itself.
  MediaGalleriesPermissionController(content::WebContents* web_contents,
                                     const extensions::Extension& extension,
                                     base::OnceClosure on_finish);

  MediaGalleriesPermissionController(
      const MediaGalleriesPermissionController&) = delete;
  MediaGalleriesPermissionController& operator=(
      const MediaGalleriesPermissionController&) = delete;

  // MediaGalleriesDialogController implementation.
  std::u16string GetHeader() const override;
  std::u16string GetSubtext() const override;
  bool IsAcceptAllowed() const override;
  std::vector<std::u16string> GetSectionHeaders() const override;
  Entries GetSectionEntries(size_t index) const override;
  // Auxiliary button for this dialog is the 'Add Folder' button.
  std::u16string GetAuxiliaryButtonText() const override;
  void DidClickAuxiliaryButton() override;
  void DidToggleEntry(GalleryDialogId gallery_id, bool selected) override;
  void DidForgetEntry(GalleryDialogId gallery_id) override;
  std::u16string GetAcceptButtonText() const override;
  void DialogFinished(bool accepted) override;
  ui::MenuModel* GetContextMenu(GalleryDialogId gallery_id) override;
  content::WebContents* WebContents() override;

 protected:
  friend class MediaGalleriesPermissionControllerTest;

  typedef base::OnceCallback<MediaGalleriesDialog*(
      MediaGalleriesDialogController*)>
      CreateDialogCallback;

  // For use with tests.
  MediaGalleriesPermissionController(
      const extensions::Extension& extension,
      MediaGalleriesPreferences* preferences,
      CreateDialogCallback create_dialog_callback,
      base::OnceClosure on_finish);

  ~MediaGalleriesPermissionController() override;

 private:
  // This type keeps track of media galleries already known to the prefs system.
  typedef std::map<GalleryDialogId, Entry> GalleryPermissionsMap;
  typedef std::map<GalleryDialogId, bool /*permitted*/> ToggledGalleryMap;

  class DialogIdMap {
   public:
    DialogIdMap();

    DialogIdMap(const DialogIdMap&) = delete;
    DialogIdMap& operator=(const DialogIdMap&) = delete;

    ~DialogIdMap();
    GalleryDialogId GetDialogId(MediaGalleryPrefId pref_id);
    MediaGalleryPrefId GetPrefId(GalleryDialogId id) const;

   private:
    GalleryDialogId next_dialog_id_;
    std::map<MediaGalleryPrefId, GalleryDialogId> back_map_;
    std::vector<MediaGalleryPrefId> forward_mapping_;
  };


  // Bottom half of constructor -- called when |preferences_| is initialized.
  void OnPreferencesInitialized();

  // SelectFileDialog::Listener implementation:
  void FileSelected(const ui::SelectedFileInfo& file_info, int index) override;
  void FileSelectionCanceled() override;

  // RemovableStorageObserver implementation.
  // Used to keep dialog in sync with removable device status.
  void OnRemovableStorageAttached(
      const storage_monitor::StorageInfo& info) override;
  void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) override;

  // MediaGalleriesPreferences::GalleryChangeObserver implementations.
  // Used to keep the dialog in sync when the preferences change.
  void OnPermissionAdded(MediaGalleriesPreferences* pref,
                         const std::string& extension_id,
                         MediaGalleryPrefId pref_id) override;
  void OnPermissionRemoved(MediaGalleriesPreferences* pref,
                           const std::string& extension_id,
                           MediaGalleryPrefId pref_id) override;
  void OnGalleryAdded(MediaGalleriesPreferences* pref,
                      MediaGalleryPrefId pref_id) override;
  void OnGalleryRemoved(MediaGalleriesPreferences* pref,
                        MediaGalleryPrefId pref_id) override;
  void OnGalleryInfoUpdated(MediaGalleriesPreferences* pref,
                            MediaGalleryPrefId pref_id) override;

  // Populates |known_galleries_| from |preferences_|. Subsequent calls merge
  // into |known_galleries_| and do not change permissions for user toggled
  // galleries.
  void InitializePermissions();

  // Saves state of |known_galleries_|, |new_galleries_| and
  // |forgotten_galleries_| to model.
  //
  // NOTE: possible states for a gallery:
  //   K   N   F   (K = Known, N = New, F = Forgotten)
  // +---+---+---+
  // | Y | N | N |
  // +---+---+---+
  // | N | Y | N |
  // +---+---+---+
  // | Y | N | Y |
  // +---+---+---+
  void SavePermissions();

  // Updates the model and view when |preferences_| changes. Some of the
  // possible changes includes a gallery getting blocklisted, or a new
  // auto detected gallery becoming available.
  void UpdateGalleriesOnPreferencesEvent();

  // Updates the model and view when a device is attached or detached.
  void UpdateGalleriesOnDeviceEvent(const std::string& device_id);

  GalleryDialogId GetDialogId(MediaGalleryPrefId pref_id);
  MediaGalleryPrefId GetPrefId(GalleryDialogId id) const;

  Profile* GetProfile();

  // The web contents from which the request originated.
  raw_ptr<content::WebContents> web_contents_;

  // This is just a reference, but it's assumed that it won't become invalid
  // while the dialog is showing.
  raw_ptr<const extensions::Extension> extension_;

  // Mapping between pref ids and dialog ids.
  DialogIdMap id_map_;

  // This map excludes those galleries which have been blocklisted; it only
  // counts active known galleries.
  GalleryPermissionsMap known_galleries_;

  // Galleries in |known_galleries_| that the user have toggled.
  ToggledGalleryMap toggled_galleries_;

  // The current set of permitted galleries (according to prefs).
  MediaGalleryPrefIdSet pref_permitted_galleries_;

  // Map of new galleries the user added, but have not saved. This list should
  // never overlap with |known_galleries_|.
  GalleryPermissionsMap new_galleries_;

  // Galleries in |known_galleries_| that the user has forgotten.
  std::set<GalleryDialogId> forgotten_galleries_;

  // Callback to run when the dialog closes.
  base::OnceClosure on_finish_;

  // The model that tracks galleries and extensions' permissions.
  // This is the authoritative source for gallery information.
  raw_ptr<MediaGalleriesPreferences> preferences_;

  // The view that's showing.
  std::unique_ptr<MediaGalleriesDialog> dialog_;

  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;

  std::unique_ptr<MediaGalleryContextMenu> context_menu_;

  // Creates the dialog. Only changed for unit tests.
  CreateDialogCallback create_dialog_callback_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_PERMISSION_CONTROLLER_H_
