// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using MimeHandlerOptionsUpdateTest = ExtensionServiceTestWithInstall;

// Options stored in an earlier browser session must be dropped by an update
// that stops handling the MIME type, even when nothing has asked for MIME
// handlers in this session.
TEST_F(MimeHandlerOptionsUpdateTest, UpdateForgetsOptionsOfDroppedMimeType) {
  InitializeEmptyExtensionService();
  static constexpr char kManifest[] =
      R"({
           "name": "MIME handler",
           "version": "%s",
           "manifest_version": 3%s
         })";
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(
      base::StringPrintf(kManifest, "1",
                         R"(, "mime_types_handler": {"application/pdf":)"
                         R"( {"handler_url": "viewer.html"}})"));
  base::FilePath crx_v1 = extension_dir.Pack("v1.crx");
  extension_dir.WriteManifest(base::StringPrintf(kManifest, "2", ""));
  base::FilePath crx_v2 = extension_dir.Pack("v2.crx");

  const Extension* extension = InstallCRX(crx_v1, INSTALL_NEW);
  ASSERT_TRUE(extension);
  const ExtensionId id = extension->id();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  prefs->UpdateExtensionPref(
      id, "mime_handler_options",
      base::test::ParseJson(R"({"application/pdf": {"enabled": false}})"));

  ASSERT_TRUE(InstallCRX(crx_v2, INSTALL_UPDATED));

  const base::DictValue* options =
      prefs->ReadPrefAsDict(id, "mime_handler_options");
  ASSERT_TRUE(options);
  EXPECT_THAT(*options, base::test::IsJson("{}"));
}

}  // namespace extensions
