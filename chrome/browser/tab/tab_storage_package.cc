// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_package.h"

#include "base/token.h"
#include "chrome/browser/tab/android_tab_package.h"

namespace tabs {

TabStoragePackage::TabStoragePackage(
    int id,
    int parent_id,
    int user_agent,
    std::unique_ptr<base::Token> tab_group_id,
    bool is_pinned,
    std::unique_ptr<AndroidTabPackage> android_tab_package)
    : id_(id),
      parent_id_(parent_id),
      user_agent_(user_agent),
      tab_group_id_(std::move(tab_group_id)),
      is_pinned_(is_pinned),
      android_tab_package_(std::move(android_tab_package)) {}

TabStoragePackage::~TabStoragePackage() = default;

}  // namespace tabs
