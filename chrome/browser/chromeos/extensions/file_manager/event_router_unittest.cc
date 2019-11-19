// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {

TEST(EventRouterTest, PopulateCrostiniUnshareEvent) {
  extensions::api::file_manager_private::CrostiniEvent event;
  EventRouter::PopulateCrostiniUnshareEvent(event, "vmname", "extensionid",
                                            "mountname", "filesystemname",
                                            "/full/path");

  EXPECT_EQ(event.event_type,
            extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE);
  EXPECT_EQ(event.vm_name, "vmname");
  EXPECT_EQ(event.entries.size(), 1u);
  base::DictionaryValue props;
  props.SetString(
      "fileSystemRoot",
      "filesystem:chrome-extension://extensionid/external/mountname/");
  props.SetString("fileSystemName", "filesystemname");
  props.SetString("fileFullPath", "/full/path");
  props.SetBoolean("fileIsDirectory", true);
  EXPECT_EQ(event.entries[0].additional_properties, props);
}

}  // namespace file_manager
