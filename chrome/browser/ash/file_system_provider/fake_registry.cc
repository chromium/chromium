// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/fake_registry.h"

#include <memory>

#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/watcher.h"

namespace ash::file_system_provider {

FakeRegistry::FakeRegistry() = default;
FakeRegistry::~FakeRegistry() = default;

void FakeRegistry::RememberFileSystem(
    const ProvidedFileSystemInfo& file_system_info,
    const Watchers& watchers) {
  file_system_info_ =
      std::make_unique<ProvidedFileSystemInfo>(file_system_info);
  watchers_ = std::make_unique<Watchers>(watchers);
}

void FakeRegistry::ForgetFileSystem(const ProviderId& provider_id,
                                    const std::string& file_system_id) {
  if (!file_system_info_.get() || !watchers_.get())
    return;
  if (file_system_info_->provider_id() == provider_id &&
      file_system_info_->file_system_id() == file_system_id) {
    file_system_info_.reset();
    watchers_.reset();
  }
}

std::unique_ptr<RegistryInterface::RestoredFileSystems>
FakeRegistry::RestoreFileSystems(const ProviderId& provider_id) {
  std::unique_ptr<RestoredFileSystems> result(new RestoredFileSystems);

  if (file_system_info_.get() && watchers_.get()) {
    RestoredFileSystem restored_file_system;
    restored_file_system.provider_id = file_system_info_->provider_id();

    MountOptions options;
    options.file_system_id = file_system_info_->file_system_id();
    options.display_name = file_system_info_->display_name();
    options.writable = file_system_info_->writable();
    options.supports_notify_tag = file_system_info_->supports_notify_tag();
    restored_file_system.options = options;
    restored_file_system.watchers = *watchers_.get();

    result->push_back(restored_file_system);
  }

  return result;
}

void FakeRegistry::UpdateWatcherTag(
    const ProvidedFileSystemInfo& file_system_info,
    const Watcher& watcher) {
  const Watchers::iterator it =
      watchers_->find(WatcherKey(watcher.entry_path, watcher.recursive));
  it->second.last_tag = watcher.last_tag;
}

const ProvidedFileSystemInfo* FakeRegistry::file_system_info() const {
  return file_system_info_.get();
}
const Watchers* FakeRegistry::watchers() const {
  return watchers_.get();
}

}  // namespace ash::file_system_provider
