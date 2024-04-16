// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_ENUMERATOR_H_
#define CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_ENUMERATOR_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

class Profile;

namespace ash {

// Searches the user's files for jpg and png images. This is used for
// displaying images that the user could select as a custom wallpaper.
// TODO(crbug.com/40562168): Add metrics on the number of files retrieved, and
// support getting paths incrementally in case the user has a large number of
// local images.
void EnumerateLocalWallpaperFiles(
    Profile* profile,
    base::OnceCallback<void(const std::vector<base::FilePath>&)> callback);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_ENUMERATOR_H_
