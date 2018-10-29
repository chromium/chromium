// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"

namespace content {
class WebContents;
}

namespace ui {
class MenuModel;
}

class MediaGalleriesDialogController;

// The view.
class MediaGalleriesDialog {
 public:
  virtual ~MediaGalleriesDialog();

  // Tell the dialog to update its display list of galleries.
  virtual void UpdateGalleries() = 0;

  // Constructs a platform-specific dialog owned and controlled by |controller|.
  static MediaGalleriesDialog* Create(
      MediaGalleriesDialogController* controller);
 private:
  friend class TestMediaGalleriesAddScanResultsFunction;

  virtual void AcceptDialogForTesting() = 0;
};

// Multiple dialog controllers are based on this interface.
// Implementations of this controller interface are responsible for handling
// the logic of the dialog and interfacing with the model (i.e.,
// MediaGalleriesPreferences). It shows the dialog and owns itself.
class MediaGalleriesDialogController {
 public:
  struct Entry {
    Entry(const MediaGalleryPrefInfo& pref_info, bool selected)
        : pref_info(pref_info),
          selected(selected) {
    }
    Entry() {}

    MediaGalleryPrefInfo pref_info;
    bool selected;
  };

  typedef std::vector<Entry> Entries;

  // The title of the dialog view.
  virtual base::string16 GetHeader() const = 0;

  // Explanatory text directly below the title.
  virtual base::string16 GetSubtext() const = 0;

  // Initial state of whether the dialog's confirmation button will be enabled.
  virtual bool IsAcceptAllowed() const = 0;

  // The titles for different sections of entries. Empty hides the header.
  virtual std::vector<base::string16> GetSectionHeaders() const = 0;

  // Get the set of permissions for the |index|th section. The size of the
  // vector returned from GetSectionHeaders() defines the number of sections.
  virtual Entries GetSectionEntries(size_t index) const = 0;

  // The text for an auxiliary button. Empty hides the button.
  virtual base::string16 GetAuxiliaryButtonText() const = 0;

  // Called when an auxiliary button is clicked.
  virtual void DidClickAuxiliaryButton() = 0;

  // An entry checkbox was toggled.
  virtual void DidToggleEntry(MediaGalleryPrefId id, bool selected) = 0;

  // The forget command in the context menu was selected.
  virtual void DidForgetEntry(MediaGalleryPrefId id) = 0;

  // The text for the accept button.
  virtual base::string16 GetAcceptButtonText() const = 0;

  // The dialog is being deleted.
  virtual void DialogFinished(bool accepted) = 0;

  virtual ui::MenuModel* GetContextMenu(MediaGalleryPrefId id) = 0;

  virtual content::WebContents* WebContents() = 0;

 protected:
  MediaGalleriesDialogController();
  virtual ~MediaGalleriesDialogController();

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesDialogController);
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_H_
