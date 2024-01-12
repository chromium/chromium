// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/provided_file_system_observer.h"

namespace ash::file_system_provider {

ProvidedFileSystemObserver::Change::Change()
    : change_type(storage::WatcherManager::CHANGED) {
}

ProvidedFileSystemObserver::Change::~Change() = default;

}  // namespace ash::file_system_provider
