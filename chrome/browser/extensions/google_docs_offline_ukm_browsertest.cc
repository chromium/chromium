// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace extensions {

// Loads a page that imports a script from Google Docs Offline extension and
// checks for UKM collection.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       TestGoogleDocsOfflineExtensionResourceImport) {
  ASSERT_TRUE(embedded_test_server()->Start());
  using UkmEntry = ukm::builders::GoogleDocsOfflineExtension;
  const GURL url(embedded_test_server()->GetURL(
      "/import_docs_offline_extension_resource.html"));

  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);

  // Imports are double counted, once in preload and once in actual load.
  EXPECT_EQ(2u, entries.size());
}

// Loads a page that fetches a script from Google Docs Offline extension and
// checks for UKM collection.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       TestGoogleDocsOfflineExtensionResourceFetch) {
  ASSERT_TRUE(embedded_test_server()->Start());
  using UkmEntry = ukm::builders::GoogleDocsOfflineExtension;
  const GURL url(embedded_test_server()->GetURL(
      "/fetch_docs_offline_extension_resource.html"));

  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
}

// Loads a page that imports a script from an extension other than Google Docs
// Offline and checks for UKM collection.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       TestNoneGoogleDocsOfflineExtensionResourceUse) {
  ASSERT_TRUE(embedded_test_server()->Start());
  using UkmEntry = ukm::builders::GoogleDocsOfflineExtension;
  const GURL url(embedded_test_server()->GetURL(
      "/import_none_docs_offline_extension_resource.html"));

  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_TRUE(entries.empty());
}

}  // namespace extensions
