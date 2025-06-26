// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/uuid.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/base/window_open_disposition.h"

namespace extensions {

using ExtensionApiFrameIdMapBrowserTest = ExtensionBrowserTest;

// Tests that extension frames have unique context IDs.
IN_PROC_BROWSER_TEST_F(ExtensionApiFrameIdMapBrowserTest, ContextIdsAreUnique) {
  static constexpr char kManifest[] =
      R"({
           "name": "My extension",
           "manifest_version": 3,
           "version": "0.1"
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page1.html"), "<html>Foo</html>");
  test_dir.WriteFile(FILE_PATH_LITERAL("page2.html"), "<html>Bar</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Open three frames, two of which point to page1.html.
  content::RenderFrameHost* page1_a_host =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension->GetResourceURL("page1.html"),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(page1_a_host);

  content::RenderFrameHost* page1_b_host =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension->GetResourceURL("page1.html"),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(page1_b_host);

  content::RenderFrameHost* page2_host =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension->GetResourceURL("page2.html"),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(page2_host);

  base::Uuid page1_a_context_id =
      ExtensionApiFrameIdMap::GetContextId(page1_a_host);
  base::Uuid page1_b_context_id =
      ExtensionApiFrameIdMap::GetContextId(page1_b_host);
  base::Uuid page2_context_id =
      ExtensionApiFrameIdMap::GetContextId(page2_host);

  // Re-fetching the IDs for the same host should return the same result.
  EXPECT_EQ(page1_a_context_id,
            ExtensionApiFrameIdMap::GetContextId(page1_a_host));
  EXPECT_EQ(page1_b_context_id,
            ExtensionApiFrameIdMap::GetContextId(page1_b_host));
  EXPECT_EQ(page2_context_id, ExtensionApiFrameIdMap::GetContextId(page2_host));

  // All three frames should have unique IDs (even though two show the same
  // resource).
  EXPECT_NE(page1_a_context_id, page1_b_context_id);
  EXPECT_NE(page1_a_context_id, page2_context_id);
  EXPECT_NE(page1_b_context_id, page2_context_id);

  // Navigate page2 to page2 (again). It should have a new (unique) context ID
  // since it's a new document.
  content::RenderFrameHost* page2_new_host =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), extension->GetResourceURL("page2.html"),
          WindowOpenDisposition::CURRENT_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(page2_new_host);
  base::Uuid page2_new_context_id =
      ExtensionApiFrameIdMap::GetContextId(page2_new_host);

  EXPECT_NE(page2_new_context_id, page1_a_context_id);
  EXPECT_NE(page2_new_context_id, page1_b_context_id);
  EXPECT_NE(page2_new_context_id, page2_context_id);
}

}  // namespace extensions
