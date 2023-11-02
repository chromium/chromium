// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

namespace extensions {
class Extension;
}

namespace registry_util {
class RegistryOverrideManager;
}

class Profile;

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile);

class EnsureMediaDirectoriesExists {
 public:
  EnsureMediaDirectoriesExists();

  EnsureMediaDirectoriesExists(const EnsureMediaDirectoriesExists&) = delete;
  EnsureMediaDirectoriesExists& operator=(const EnsureMediaDirectoriesExists&) =
      delete;

  ~EnsureMediaDirectoriesExists();

  int num_galleries() const { return num_galleries_; }

  base::FilePath GetFakeAppDataPath() const;

  // Changes the directories for the media paths (music, pictures, videos)
  // overrides to new, different directories that are generated.
  void ChangeMediaPathOverrides();
#if BUILDFLAG(IS_WIN)
  base::FilePath GetFakeLocalAppDataPath() const;
#endif

 private:
  void Init();

  base::ScopedTempDir fake_dir_;

  int num_galleries_;

  int times_overrides_changed_;

  std::unique_ptr<base::ScopedPathOverride> app_data_override_;
  std::unique_ptr<base::ScopedPathOverride> music_override_;
  std::unique_ptr<base::ScopedPathOverride> pictures_override_;
  std::unique_ptr<base::ScopedPathOverride> video_override_;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<base::ScopedPathOverride> local_app_data_override_;

  registry_util::RegistryOverrideManager registry_override_;
#endif
};

extern base::FilePath MakeMediaGalleriesTestingPath(const std::string& dir);

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_TEST_UTIL_H_
