// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_

#include <string>

#include "extensions/common/extension_id.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ExternalCacheDelegate {
 public:
  virtual ~ExternalCacheDelegate() = default;

  // Caller owns |prefs|.
  virtual void OnExtensionListsUpdated(const base::DictionaryValue* prefs);

  // Called after extension with |id| is loaded in cache.
  virtual void OnExtensionLoadedInCache(const extensions::ExtensionId& id);

  // Called when extension with |id| fails to load due to a download error.
  virtual void OnExtensionDownloadFailed(const extensions::ExtensionId& id);

  // Called when the cached .crx file for |id| is deleted (e.g. due to failed
  // install / corrupted file).
  virtual void OnCachedExtensionFileDeleted(const extensions::ExtensionId& id);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_
