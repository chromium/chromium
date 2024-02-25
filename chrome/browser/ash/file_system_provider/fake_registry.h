// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_REGISTRY_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_REGISTRY_H_

#include "chrome/browser/ash/file_system_provider/registry_interface.h"

#include "chrome/browser/ash/file_system_provider/watcher.h"

namespace ash::file_system_provider {

class ProvidedFileSystemInfo;

// Fake implementation of the registry.
// For simplicity it can remember at most only one file system.
class FakeRegistry : public RegistryInterface {
 public:
  FakeRegistry();

  FakeRegistry(const FakeRegistry&) = delete;
  FakeRegistry& operator=(const FakeRegistry&) = delete;

  ~FakeRegistry() override;
  void RememberFileSystem(const ProvidedFileSystemInfo& file_system_info,
                          const Watchers& watchers) override;
  void ForgetFileSystem(const ProviderId& provider_id,
                        const std::string& file_system_id) override;
  std::unique_ptr<RestoredFileSystems> RestoreFileSystems(
      const ProviderId& provider_id) override;
  void UpdateWatcherTag(const ProvidedFileSystemInfo& file_system_info,
                        const Watcher& watcher) override;
  const ProvidedFileSystemInfo* file_system_info() const;
  const Watchers* watchers() const;

 private:
  std::unique_ptr<ProvidedFileSystemInfo> file_system_info_;
  std::unique_ptr<Watchers> watchers_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FAKE_REGISTRY_H_
