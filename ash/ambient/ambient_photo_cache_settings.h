// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CACHE_SETTINGS_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CACHE_SETTINGS_H_

#include "ash/ash_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

ASH_EXPORT const base::FilePath& GetAmbientPhotoCacheRootDir();
ASH_EXPORT const base::FilePath& GetAmbientBackupPhotoCacheRootDir();

ASH_EXPORT void SetAmbientPhotoCacheRootDirForTesting(base::FilePath root_dir);
ASH_EXPORT void SetAmbientBackupPhotoCacheRootDirForTesting(
    base::FilePath root_dir);

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CACHE_SETTINGS_H_
