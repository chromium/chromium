// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace file_manager {

TEST(EventRouterTest, PopulateCrostiniEvent) {
  extensions::api::file_manager_private::CrostiniEvent ext_event;
  url::Origin ext_origin = url::Origin::Create(
      extensions::Extension::GetBaseURLFromExtensionId("extensionid"));
  EventRouter::PopulateCrostiniEvent(
      ext_event,
      extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE,
      "vmname", ext_origin, "mountname", "filesystemname", "/full/path");

  EXPECT_EQ(ext_event.event_type,
            extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE);
  EXPECT_EQ(ext_event.vm_name, "vmname");
  EXPECT_EQ(ext_event.entries.size(), 1u);
  base::DictionaryValue ext_props;
  ext_props.SetStringKey(
      "fileSystemRoot",
      "filesystem:chrome-extension://extensionid/external/mountname/");
  ext_props.SetStringKey("fileSystemName", "filesystemname");
  ext_props.SetStringKey("fileFullPath", "/full/path");
  ext_props.SetBoolKey("fileIsDirectory", true);
  EXPECT_EQ(ext_event.entries[0].additional_properties, ext_props);

  extensions::api::file_manager_private::CrostiniEvent swa_event;
  url::Origin swa_origin = url::Origin::Create(
      GURL("chrome://file-manager/this-part-should-not-be-in?the=event"));
  EventRouter::PopulateCrostiniEvent(
      swa_event,
      extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_SHARE,
      "vmname", swa_origin, "mountname", "filesystemname", "/full/path");

  EXPECT_EQ(swa_event.event_type,
            extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_SHARE);
  EXPECT_EQ(swa_event.vm_name, "vmname");
  EXPECT_EQ(swa_event.entries.size(), 1u);
  base::DictionaryValue swa_props;
  swa_props.SetStringKey(
      "fileSystemRoot", "filesystem:chrome://file-manager/external/mountname/");
  swa_props.SetStringKey("fileSystemName", "filesystemname");
  swa_props.SetStringKey("fileFullPath", "/full/path");
  swa_props.SetBoolKey("fileIsDirectory", true);
  EXPECT_EQ(swa_event.entries[0].additional_properties, swa_props);
}

}  // namespace file_manager
