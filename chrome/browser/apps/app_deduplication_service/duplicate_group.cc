// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/duplicate_group.h"

namespace apps::deduplication {

DuplicateGroup::DuplicateGroup() = default;
DuplicateGroup::~DuplicateGroup() = default;
DuplicateGroup::DuplicateGroup(DuplicateGroup&&) = default;
DuplicateGroup& DuplicateGroup::operator=(DuplicateGroup&&) = default;

}  // namespace apps::deduplication
