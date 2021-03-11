// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, I18N) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("i18n")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, I18NUpdate) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Create an Extension whose messages.json file will be updated.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());
  base::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("manifest.json"),
      extension_dir.GetPath().AppendASCII("manifest.json"));
  base::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("contentscript.js"),
      extension_dir.GetPath().AppendASCII("contentscript.js"));
  base::CopyDirectory(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("_locales"),
      extension_dir.GetPath().AppendASCII("_locales"), true);

  const Extension* extension = LoadExtension(extension_dir.GetPath());

  ResultCatcher catcher;

  // Test that the messages.json file is loaded and the i18n message is loaded.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));
  EXPECT_TRUE(catcher.GetNextResult());

  std::u16string title;
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(std::string("FIRSTMESSAGE"), base::UTF16ToUTF8(title));

  // Change messages.json file and reload extension.
  base::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate").AppendASCII("messages2.json"),
      extension_dir.GetPath().AppendASCII("_locales/en/messages.json"));
  ReloadExtension(extension->id());

  // Check that the i18n message is also changed.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));
  EXPECT_TRUE(catcher.GetNextResult());

  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(std::string("SECONDMESSAGE"), base::UTF16ToUTF8(title));
}

}  // namespace extensions
