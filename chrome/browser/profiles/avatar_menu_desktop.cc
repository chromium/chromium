// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include <stddef.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"

// static
AvatarMenu::ImageLoadStatus AvatarMenu::GetImageForMenuButton(
    const base::FilePath& profile_path,
    gfx::Image* image,
    int preferred_size) {
  if (!g_browser_process->profile_manager())
    return ImageLoadStatus::BROWSER_SHUTTING_DOWN;
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    // This can happen if the user deletes the current profile.
    return ImageLoadStatus::PROFILE_DELETED;
  }

  ImageLoadStatus status = ImageLoadStatus::LOADED;
  // We need to specifically report GAIA images that are not available yet.
  if (entry->IsUsingGAIAPicture() && !entry->GetGAIAPicture()) {
    if (entry->IsGAIAPictureLoaded())
      status = ImageLoadStatus::MISSING;
    else
      status = ImageLoadStatus::LOADING;
  }

  *image = entry->GetAvatarIcon(preferred_size, /*use_high_res_file=*/false,
                                /*icon_params=*/{.has_padding = false});
  return status;
}
