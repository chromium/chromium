// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_photo_cache_settings.h"

#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"

namespace ash {

namespace {

base::FilePath GetCacheRootPath() {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(FILE_PATH_LITERAL(kAmbientModeDirectoryName));
}

base::FilePath& GetPrimaryRootDir() {
  static base::NoDestructor<base::FilePath> g_root_dir(
      GetCacheRootPath().Append(
          FILE_PATH_LITERAL(kAmbientModeCacheDirectoryName)));
  return *g_root_dir;
}

base::FilePath& GetBackupRootDir() {
  static base::NoDestructor<base::FilePath> g_root_dir(
      GetCacheRootPath().Append(
          FILE_PATH_LITERAL(kAmbientModeBackupCacheDirectoryName)));
  return *g_root_dir;
}

}  // namespace

const base::FilePath& GetAmbientPhotoCacheRootDir() {
  return GetPrimaryRootDir();
}

const base::FilePath& GetAmbientBackupPhotoCacheRootDir() {
  return GetBackupRootDir();
}

void SetAmbientPhotoCacheRootDirForTesting(base::FilePath root_dir) {
  CHECK_IS_TEST();
  GetPrimaryRootDir() = std::move(root_dir);
}

void SetAmbientBackupPhotoCacheRootDirForTesting(base::FilePath root_dir) {
  CHECK_IS_TEST();
  GetBackupRootDir() = std::move(root_dir);
}

}  // namespace ash
