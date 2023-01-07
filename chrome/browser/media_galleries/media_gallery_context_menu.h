// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERY_CONTEXT_MENU_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERY_CONTEXT_MENU_H_

#include "base/functional/callback.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "ui/base/models/simple_menu_model.h"

class MediaGalleryContextMenu : public ui::SimpleMenuModel,
                                public ui::SimpleMenuModel::Delegate {
 public:
  typedef base::RepeatingCallback<void(MediaGalleryPrefId pref_id)>
      ForgetGalleryCallback;

  explicit MediaGalleryContextMenu(const ForgetGalleryCallback& callback);

  MediaGalleryContextMenu(const MediaGalleryContextMenu&) = delete;
  MediaGalleryContextMenu& operator=(const MediaGalleryContextMenu&) = delete;

  ~MediaGalleryContextMenu() override;

  void set_pref_id(MediaGalleryPrefId pref_id) {
    pref_id_ = pref_id;
  }

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  MediaGalleryPrefId pref_id_;
  ForgetGalleryCallback callback_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERY_CONTEXT_MENU_H_
