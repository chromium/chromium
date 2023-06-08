// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_gallery_context_menu.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

MediaGalleryContextMenu::MediaGalleryContextMenu(
    const ForgetGalleryCallback& callback)
    : ui::SimpleMenuModel(this), callback_(callback) {
  AddItem(1, l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_DELETE));
}

MediaGalleryContextMenu::~MediaGalleryContextMenu() {}

bool MediaGalleryContextMenu::IsCommandIdChecked(int command_id) const {
  return false;
}

bool MediaGalleryContextMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool MediaGalleryContextMenu::IsCommandIdVisible(int command_id) const {
  return true;
}

void MediaGalleryContextMenu::ExecuteCommand(int command_id, int event_flags) {
  callback_.Run(pref_id_);
}
