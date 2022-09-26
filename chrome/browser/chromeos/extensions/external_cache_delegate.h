// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_

#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ExternalCacheDelegate {
 public:
  virtual ~ExternalCacheDelegate() = default;

  // Legacy function until the migration (https://crbug.com/1366865) is done.
  // At most one of the two functions could be overridden in the child classes.
  // Prefer the one with the base::Value::Dict
  // Caller owns |prefs|.
  virtual void OnExtensionListsUpdated(const base::DictionaryValue* prefs);
  // Caller owns |prefs|.
  virtual void OnExtensionListsUpdated(const base::Value::Dict& prefs);

  // Called after extension with |id| is loaded in cache.
  virtual void OnExtensionLoadedInCache(const extensions::ExtensionId& id);

  // Called when extension with |id| fails to load due to a download error.
  virtual void OnExtensionDownloadFailed(const extensions::ExtensionId& id);

  // Called when the cached .crx file for |id| is deleted (e.g. due to failed
  // install / corrupted file).
  virtual void OnCachedExtensionFileDeleted(const extensions::ExtensionId& id);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::ExternalCacheDelegate;
}

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_
