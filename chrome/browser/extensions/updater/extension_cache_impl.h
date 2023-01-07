// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_CACHE_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_CACHE_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/updater/extension_cache.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace extensions {

class ChromeOSExtensionCacheDelegate;
class LocalExtensionCache;

// Singleton call that caches extensions .crx files to share them between
// multiple users and profiles on the machine.
class ExtensionCacheImpl : public ExtensionCache {
 public:
  explicit ExtensionCacheImpl(
      std::unique_ptr<ChromeOSExtensionCacheDelegate> delegate);

  ExtensionCacheImpl(const ExtensionCacheImpl&) = delete;
  ExtensionCacheImpl& operator=(const ExtensionCacheImpl&) = delete;

  ~ExtensionCacheImpl() override;

  // Implementation of ExtensionCache.
  void Start(base::OnceClosure callback) override;
  void Shutdown(base::OnceClosure callback) override;
  void AllowCaching(const std::string& id) override;
  bool GetExtension(const std::string& id,
                    const std::string& expected_hash,
                    base::FilePath* file_path,
                    std::string* version) override;
  void PutExtension(const std::string& id,
                    const std::string& expected_hash,
                    const base::FilePath& file_path,
                    const std::string& version,
                    PutExtensionCallback callback) override;
  bool OnInstallFailed(const std::string& id,
                       const std::string& hash,
                       const CrxInstallError& error) override;

 private:
  // Callback that is called when local cache is ready.
  void OnCacheInitialized();

  // Check if this extension is allowed to be cached.
  bool CachingAllowed(const std::string& id);

  // Cache implementation that uses local cache dir.
  std::unique_ptr<LocalExtensionCache> cache_;

  // Set of extensions that can be cached.
  std::set<std::string> allowed_extensions_;

  // List of callbacks that should be called when the cache is ready.
  std::vector<base::OnceClosure> init_callbacks_;

  // Weak factory for callbacks.
  base::WeakPtrFactory<ExtensionCacheImpl> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_CACHE_IMPL_H_
