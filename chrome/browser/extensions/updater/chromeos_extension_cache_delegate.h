// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_CHROMEOS_EXTENSION_CACHE_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_CHROMEOS_EXTENSION_CACHE_DELEGATE_H_

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace extensions {

// Chrome OS-specific implementation, which has a pre-defined extension cache
// path and a policy-configurable maximum cache size.
class ChromeOSExtensionCacheDelegate {
 public:
  ChromeOSExtensionCacheDelegate();
  explicit ChromeOSExtensionCacheDelegate(const base::FilePath& cache_dir);

  ChromeOSExtensionCacheDelegate(const ChromeOSExtensionCacheDelegate&) =
      delete;
  ChromeOSExtensionCacheDelegate& operator=(
      const ChromeOSExtensionCacheDelegate&) = delete;

  const base::FilePath& GetCacheDir() const;
  size_t GetMinimumCacheSize() const;
  size_t GetMaximumCacheSize() const;
  base::TimeDelta GetMaximumCacheAge() const;

 private:
  const base::FilePath cache_dir_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_CHROMEOS_EXTENSION_CACHE_DELEGATE_H_
