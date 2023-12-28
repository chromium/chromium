// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_

#include "base/values.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_id.h"

namespace chromeos {

class ExternalCacheDelegate {
 public:
  virtual ~ExternalCacheDelegate() = default;

  // Caller owns |prefs|.
  virtual void OnExtensionListsUpdated(const base::Value::Dict& prefs);

  // Called after extension with |id| is loaded in cache. |is_updated| indicates
  // whether the extension is updated.
  virtual void OnExtensionLoadedInCache(const extensions::ExtensionId& id,
                                        bool is_updated);

  // Called when extension with |id| fails to load due to a download error.
  virtual void OnExtensionDownloadFailed(
      const extensions::ExtensionId& id,
      extensions::ExtensionDownloaderDelegate::Error error);

  // Called when the cached .crx file for |id| is deleted (e.g. due to failed
  // install / corrupted file).
  virtual void OnCachedExtensionFileDeleted(const extensions::ExtensionId& id);

  // Called to check if extension rollback is allowed by this delegate.
  virtual bool IsRollbackAllowed() const;

  // Called to check if extension rollback can be performed now.
  virtual bool CanRollbackNow() const;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_EXTERNAL_CACHE_DELEGATE_H_
