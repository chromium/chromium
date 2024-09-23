// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif

class ChromeSigninClientBrowserTest : public InProcessBrowserTest {};

// This test is intended to make sure the count of bookmarks is done accurately.
IN_PROC_BROWSER_TEST_F(ChromeSigninClientBrowserTest,
                       BookmarksMetricsRecordOnSignin_Sync) {
  base::HistogramTester histogram_tester;

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());

  // Constructing the bookmark graph.

  // These two URLs are used twice to make sure they are counted twice as well.
  GURL url1("http://url1");
  GURL url4("http://url4");
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model->bookmark_bar_node();
  bookmark_model->AddURL(bookmark_bar, bookmark_bar->children().size(),
                         u"bookmark_bar_URL_1", url1);
  const bookmarks::BookmarkNode* bar_folder = bookmark_model->AddFolder(
      bookmark_bar, bookmark_bar->children().size(), u"bar_folder_1");
  bookmark_model->AddURL(bookmark_bar, bookmark_bar->children().size(),
                         u"bookmark_bar_URL_2", GURL("http://url2"));

  bookmark_model->AddURL(bar_folder, bar_folder->children().size(),
                         u"bar_URL_1", url4);
  const bookmarks::BookmarkNode* bar_sub_folder = bookmark_model->AddFolder(
      bar_folder, bar_folder->children().size(), u"bar_sub_folder");
  bookmark_model->AddURL(bar_sub_folder, bar_sub_folder->children().size(),
                         u"bar_sub_folderURL_1", url1);

  const bookmarks::BookmarkNode* other_bookmarks = bookmark_model->other_node();
  const bookmarks::BookmarkNode* other_folder = bookmark_model->AddFolder(
      other_bookmarks, other_bookmarks->children().size(), u"other_folder_1");
  bookmark_model->AddURL(other_bookmarks, other_bookmarks->children().size(),
                         u"other_URL_1", GURL("http://url3"));
  bookmark_model->AddURL(other_folder, other_folder->children().size(),
                         u"other_folder_URL_1", url4);

  // Bookmark graph:
  //
  // Bookmark Bar
  // |- bookmark_bar_URL_1 (url1)
  // |_ bar_folder_1
  // |  |_ bar_URL_1 (url4)
  // |  |_ bar_sub_folder
  // |  |  |- bar_sub_folderURL_1 (url1)
  // |_ bookmark_bar_URL_2 (url2)
  // Other Bookmarks
  // |_ other_folder_1
  // |  |- other_folder_URL_1 (url4)
  // |_ other_URL_1 (url3)

  // Given the graph above:
  //
  // Count all bookmarks (even duplicates, without folders).
  size_t expected_all_bookmarks_count = 6;
  // Count only first layer of the bookmark bar (including folders).
  size_t expected_bar_bookmarks_count = 3;

  // Sign in to Chrome.
  const std::string email = "alice@example.com";
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSignin);

  // Test signin histogram expectations.
  histogram_tester.ExpectUniqueSample("Signin.Bookmarks.OnSignin.AllBookmarks",
                                      expected_all_bookmarks_count, 1);
  histogram_tester.ExpectUniqueSample("Signin.Bookmarks.OnSignin.BookmarksBar",
                                      expected_bar_bookmarks_count, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Bookmarks.OnSignin.AllBookmarks.Other",
      expected_all_bookmarks_count, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Bookmarks.OnSignin.BookmarksBar.Other",
      expected_bar_bookmarks_count, 1);
  // No values expected for sync.
  base::HistogramTester::CountsMap expected_sync_counts;
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.Bookmarks.OnSync"),
      testing::ContainerEq(expected_sync_counts));

  //----------------------------------------------------------------------------

  // Add 2 empty folders before syncing.
  bookmark_model->AddFolder(bookmark_bar, bookmark_bar->children().size(),
                            u"bar_folder_2");
  // Should expect 1 more count for the bar bookmarks histograms.
  size_t sync_expected_bar_bookmarks_count_count =
      expected_bar_bookmarks_count + 1;
  // But not for the all bookmarks count.
  bookmark_model->AddFolder(other_bookmarks, other_bookmarks->children().size(),
                            u"other_folder_1");

  // New histogram tester for easier new values check.
  base::HistogramTester histogram_tester_sync;
  // Enable Sync.
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSync);

  // Test sync histogram expectations.
  histogram_tester.ExpectUniqueSample("Signin.Bookmarks.OnSync.AllBookmarks",
                                      expected_all_bookmarks_count, 1);
  histogram_tester.ExpectUniqueSample("Signin.Bookmarks.OnSync.BookmarksBar",
                                      sync_expected_bar_bookmarks_count_count,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Bookmarks.OnSync.AllBookmarks.Other",
      expected_all_bookmarks_count, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Bookmarks.OnSync.BookmarksBar.Other",
      sync_expected_bar_bookmarks_count_count, 1);

  // No new values expected for Signin histograms.
  base::HistogramTester::CountsMap expected_signin_counts;
  EXPECT_THAT(
      histogram_tester_sync.GetTotalCountsForPrefix("Signin.Bookmarks.OnSign"),
      testing::ContainerEq(expected_signin_counts));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_F(ChromeSigninClientBrowserTest,
                       ExtensionsMetricsRecordOnSignin_Sync) {
  base::HistogramTester histogram_tester;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  // Create 3 fake extensions and enable them.
  // Setting the ManifestLocation to kInternal means that the extension is user
  // installed. kComponent is an internal Chrome Exntension used for features,
  // kExternalPolicy means that the extension was installed through a policy.
  registry->AddEnabled(
      extensions::ExtensionBuilder("Extension1")
          .SetLocation(extensions::mojom::ManifestLocation::kInternal)
          .Build());
  registry->AddEnabled(
      extensions::ExtensionBuilder("Extension2")
          .SetLocation(extensions::mojom::ManifestLocation::kComponent)
          .Build());
  registry->AddEnabled(
      extensions::ExtensionBuilder("Extension3")
          .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicy)
          .Build());

  // Only one of the 3 extensions is considered user_installed.
  size_t expected_extensions_count = 1;
  // Sign in to Chrome.
  const std::string email = "alice@example.com";
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSignin);

  histogram_tester.ExpectUniqueSample("Signin.Extensions.OnSignin",
                                      expected_extensions_count, 1);
  histogram_tester.ExpectUniqueSample("Signin.Extensions.OnSignin.Other",
                                      expected_extensions_count, 1);
  // No values expected for OnSync.
  base::HistogramTester::CountsMap expected_sync_counts;
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Signin.Extensions.OnSync"),
      testing::ContainerEq(expected_sync_counts));

  // Add 1 more extension before syncing.
  registry->AddEnabled(
      extensions::ExtensionBuilder("Extension4")
          .SetLocation(extensions::mojom::ManifestLocation::kInternal)
          .Build());
  size_t sync_expected_extensions_count = expected_extensions_count + 1;

  // New histogram tester for easier new values check.
  base::HistogramTester histogram_tester_sync;
  // Enable Sync.
  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSync);

  histogram_tester_sync.ExpectUniqueSample("Signin.Extensions.OnSync",
                                           sync_expected_extensions_count, 1);
  histogram_tester_sync.ExpectUniqueSample("Signin.Extensions.OnSync.Other",
                                           sync_expected_extensions_count, 1);
  // No values expected for OnSignin.
  base::HistogramTester::CountsMap expected_signin_counts;
  EXPECT_THAT(histogram_tester_sync.GetTotalCountsForPrefix(
                  "Signin.Extensions.OnSignin"),
              testing::ContainerEq(expected_signin_counts));
}
#endif
