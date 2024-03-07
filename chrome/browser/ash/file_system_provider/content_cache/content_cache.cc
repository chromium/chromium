// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"

namespace ash::file_system_provider {

ContentCache::ContentCache(const base::FilePath& root_dir)
    : root_dir_(root_dir) {}

ContentCache::~ContentCache() = default;

}  // namespace ash::file_system_provider
