// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"

namespace ash::file_system_provider {

CacheFileContext::CacheFileContext(const std::string& version_tag)
    : version_tag(version_tag) {}

CacheFileContext::CacheFileContext(CacheFileContext&&) = default;
CacheFileContext& CacheFileContext::operator=(CacheFileContext&&) = default;

CacheFileContext::~CacheFileContext() = default;

}  // namespace ash::file_system_provider
