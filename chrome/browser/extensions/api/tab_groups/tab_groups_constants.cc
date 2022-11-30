// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_constants.h"

namespace extensions {
namespace tab_groups_constants {

const char kCannotMoveGroupIntoMiddleOfOtherGroupError[] =
    "Cannot move the group to an index that is in the middle of another group.";
const char kCannotMoveGroupIntoMiddleOfPinnedTabsError[] =
    "Cannot move the group to an index that is in the middle of pinned tabs.";
const char kGroupNotFoundError[] = "No group with id: *.";

}  // namespace tab_groups_constants
}  // namespace extensions
