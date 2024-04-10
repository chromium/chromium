// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"

#include <stddef.h>

#include "ash/constants/ash_paths.h"
#include "base/path_service.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace extensions {

ChromeOSExtensionCacheDelegate::ChromeOSExtensionCacheDelegate()
    : cache_dir_(base::PathService::CheckedGet(
          ash::DIR_DEVICE_EXTENSION_LOCAL_CACHE)) {}

ChromeOSExtensionCacheDelegate::ChromeOSExtensionCacheDelegate(
    const base::FilePath& cache_dir)
    : cache_dir_(cache_dir) {}

const base::FilePath& ChromeOSExtensionCacheDelegate::GetCacheDir() const {
  return cache_dir_;
}

size_t ChromeOSExtensionCacheDelegate::GetMinimumCacheSize() const {
  // Default minimum size of local cache on disk, in bytes.
  static constexpr int kDefaultMinimumCacheSize = 1024 * 1024;
  return kDefaultMinimumCacheSize;
}

size_t ChromeOSExtensionCacheDelegate::GetMaximumCacheSize() const {
  // Default maximum size of local cache on disk, in bytes.
  static constexpr size_t kDefaultCacheSizeLimit = 256 * 1024 * 1024;

  size_t max_size = kDefaultCacheSizeLimit;
  int policy_size = 0;
  if (ash::CrosSettings::Get()->GetInteger(ash::kExtensionCacheSize,
                                           &policy_size) &&
      policy_size >= static_cast<int>(GetMinimumCacheSize())) {
    max_size = policy_size;
  }
  return max_size;
}

base::TimeDelta ChromeOSExtensionCacheDelegate::GetMaximumCacheAge() const {
  // Maximum age of unused extensions in the cache.
  static constexpr base::TimeDelta kMaxCacheAge = base::Days(30);
  return kMaxCacheAge;
}

}  // namespace extensions
