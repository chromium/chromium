// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_test_util.h"

#include <stddef.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#endif  // BUILDFLAG(IS_WIN)

scoped_refptr<extensions::Extension> AddMediaGalleriesApp(
    const std::string& name,
    const std::vector<std::string>& media_galleries_permissions,
    Profile* profile) {
  base::Value::Dict manifest;
  manifest.Set(extensions::manifest_keys::kName, name);
  manifest.Set(extensions::manifest_keys::kVersion, "0.1");
  manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
  base::Value::List background_script_list;
  background_script_list.Append("background.js");
  manifest.SetByDottedPath(
      extensions::manifest_keys::kPlatformAppBackgroundScripts,
      std::move(background_script_list));

  base::Value::List permission_detail_list;
  for (const auto& permission : media_galleries_permissions)
    permission_detail_list.Append(permission);
  base::Value::Dict media_galleries_permission;
  media_galleries_permission.Set("mediaGalleries",
                                 std::move(permission_detail_list));
  base::Value::List permission_list;
  permission_list.Append(std::move(media_galleries_permission));
  manifest.Set(extensions::manifest_keys::kPermissions,
               std::move(permission_list));

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile);
  base::FilePath path = extension_prefs->install_directory().AppendASCII(name);
  std::string errors;
  scoped_refptr<extensions::Extension> extension =
      extensions::Extension::Create(
          path, extensions::mojom::ManifestLocation::kInternal, manifest,
          extensions::Extension::NO_FLAGS, &errors);
  EXPECT_TRUE(extension.get() != nullptr) << errors;
  EXPECT_TRUE(crx_file::id_util::IdIsValid(extension->id()));
  if (!extension.get() || !crx_file::id_util::IdIsValid(extension->id()))
    return nullptr;

  extension_prefs->OnExtensionInstalled(
      extension.get(),
      extensions::Extension::ENABLED,
      syncer::StringOrdinal::CreateInitialOrdinal(),
      std::string());
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->AddExtension(extension.get());
  extension_service->EnableExtension(extension->id());

  return extension;
}

EnsureMediaDirectoriesExists::EnsureMediaDirectoriesExists()
    : num_galleries_(0), times_overrides_changed_(0) {
  Init();
}

EnsureMediaDirectoriesExists::~EnsureMediaDirectoriesExists() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::ignore = fake_dir_.Delete();
}

void EnsureMediaDirectoriesExists::ChangeMediaPathOverrides() {
  // Each pointer must be reset an extra time so as to destroy the existing
  // override prior to creating a new one. This is because the PathService,
  // which supports these overrides, only allows one override to exist per path
  // in its internal bookkeeping; attempting to add a second override invokes
  // a CHECK crash.
  music_override_.reset();
  std::string music_path_string("music");
  music_path_string.append(base::NumberToString(times_overrides_changed_));
  music_override_ = std::make_unique<base::ScopedPathOverride>(
      chrome::DIR_USER_MUSIC,
      fake_dir_.GetPath().AppendASCII(music_path_string));

  pictures_override_.reset();
  std::string pictures_path_string("pictures");
  pictures_path_string.append(base::NumberToString(times_overrides_changed_));
  pictures_override_ = std::make_unique<base::ScopedPathOverride>(
      chrome::DIR_USER_PICTURES,
      fake_dir_.GetPath().AppendASCII(pictures_path_string));

  video_override_.reset();
  std::string videos_path_string("videos");
  videos_path_string.append(base::NumberToString(times_overrides_changed_));
  video_override_ = std::make_unique<base::ScopedPathOverride>(
      chrome::DIR_USER_VIDEOS,
      fake_dir_.GetPath().AppendASCII(videos_path_string));

  times_overrides_changed_++;

  num_galleries_ = 3;
}

base::FilePath EnsureMediaDirectoriesExists::GetFakeAppDataPath() const {
  base::ScopedAllowBlockingForTesting allow_blocking;
  DCHECK(fake_dir_.IsValid());
  return fake_dir_.GetPath().AppendASCII("appdata");
}

#if BUILDFLAG(IS_WIN)
base::FilePath EnsureMediaDirectoriesExists::GetFakeLocalAppDataPath() const {
  DCHECK(fake_dir_.IsValid());
  return fake_dir_.GetPath().AppendASCII("localappdata");
}
#endif  // BUILDFLAG(IS_WIN)

void EnsureMediaDirectoriesExists::Init() {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
  return;
#else

  ASSERT_TRUE(fake_dir_.CreateUniqueTempDir());

  ChangeMediaPathOverrides();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_ANDROID)
}

base::FilePath MakeMediaGalleriesTestingPath(const std::string& dir) {
#if BUILDFLAG(IS_WIN)
  return base::FilePath(FILE_PATH_LITERAL("C:\\")).AppendASCII(dir);
#elif BUILDFLAG(IS_POSIX)
  return base::FilePath(FILE_PATH_LITERAL("/")).Append(dir);
#else
#error Unknown platform.
#endif
}
