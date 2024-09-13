// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace {

class AutoplayMetricsBrowserTest : public InProcessBrowserTest {
 public:
  using Entry = ukm::builders::Media_Autoplay_Attempt;
  using CreatedEntry = ukm::builders::DocumentCreated;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TryAutoplay(ukm::TestUkmRecorder& ukm_recorder,
                   const content::ToRenderFrameHost& adapter) {
    base::RunLoop run_loop;
    ukm_recorder.SetOnAddEntryCallback(Entry::kEntryName,
                                       run_loop.QuitClosure());
    EXPECT_TRUE(ExecJs(adapter.render_frame_host(), "tryPlayback();",
                       content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    run_loop.Run();
  }

  void NavigateFrameAndWait(content::RenderFrameHost* rfh, const GURL& url) {
    content::TestFrameNavigationObserver observer(rfh);
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    params.frame_tree_node_id = rfh->GetFrameTreeNodeId();
    content::WebContents::FromRenderFrameHost(rfh)
        ->GetController()
        .LoadURLWithParams(params);
    observer.Wait();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* first_child() const {
    return ChildFrameAt(web_contents(), 0);
  }

  content::RenderFrameHost* second_child() const {
    return ChildFrameAt(first_child(), 0);
  }
};

// Flaky on various platforms. https://crbug.com/1101841
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_RecordAutoplayAttemptUkm DISABLED_RecordAutoplayAttemptUkm
#else
#define MAYBE_RecordAutoplayAttemptUkm RecordAutoplayAttemptUkm
#endif
IN_PROC_BROWSER_TEST_F(AutoplayMetricsBrowserTest,
                       MAYBE_RecordAutoplayAttemptUkm) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  GURL main_url(embedded_test_server()->GetURL("example.com",
                                               "/media/autoplay_iframe.html"));
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/media/autoplay_iframe.html"));
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/media/autoplay_iframe.html"));

  // Navigate main frame, try play.
  NavigateFrameAndWait(web_contents()->GetPrimaryMainFrame(), main_url);
  TryAutoplay(test_ukm_recorder, web_contents());

  // Check that we recorded a UKM event using the main frame URL.
  {
    auto ukm_entries = test_ukm_recorder.GetEntriesByName(Entry::kEntryName);

    ASSERT_EQ(1u, ukm_entries.size());
    test_ukm_recorder.ExpectEntrySourceHasUrl(ukm_entries[0], main_url);
  }

  // Navigate sub frame, try play.
  NavigateFrameAndWait(first_child(), foo_url);
  TryAutoplay(test_ukm_recorder, first_child());

  // Check that we recorded a UKM event that is not keyed to any URL.
  {
    auto ukm_entries = test_ukm_recorder.GetEntriesByName(Entry::kEntryName);

    ASSERT_EQ(2u, ukm_entries.size());
    EXPECT_FALSE(
        test_ukm_recorder.GetSourceForSourceId(ukm_entries[1]->source_id));

    // Check that a DocumentCreated entry was also created that was not keyed to
    // any URL. However, we can use the navigation source ID to link this source
    // to the top frame URL.
    auto* dc_entry = test_ukm_recorder.GetDocumentCreatedEntryForSourceId(
        ukm_entries[1]->source_id);
    EXPECT_TRUE(dc_entry);
    EXPECT_EQ(ukm_entries[1]->source_id, dc_entry->source_id);
    EXPECT_FALSE(test_ukm_recorder.GetSourceForSourceId(dc_entry->source_id));
    EXPECT_EQ(main_url,
              test_ukm_recorder
                  .GetSourceForSourceId(*test_ukm_recorder.GetEntryMetric(
                      dc_entry, CreatedEntry::kNavigationSourceIdName))
                  ->url());
    EXPECT_EQ(0, *test_ukm_recorder.GetEntryMetric(
                     dc_entry, CreatedEntry::kIsMainFrameName));
  }

  // Navigate sub sub frame, try play.
  NavigateFrameAndWait(second_child(), bar_url);
  TryAutoplay(test_ukm_recorder, second_child());

  // Check that we recorded a UKM event that is not keyed to any url.
  {
    auto ukm_entries = test_ukm_recorder.GetEntriesByName(Entry::kEntryName);

    ASSERT_EQ(3u, ukm_entries.size());
    EXPECT_FALSE(
        test_ukm_recorder.GetSourceForSourceId(ukm_entries[2]->source_id));

    // Check that a DocumentCreated entry was also created that was not keyed to
    // any URL. However, we can use the navigation source ID to link this source
    // to the top frame URL.
    auto* dc_entry = test_ukm_recorder.GetDocumentCreatedEntryForSourceId(
        ukm_entries[2]->source_id);
    EXPECT_TRUE(dc_entry);
    EXPECT_EQ(ukm_entries[2]->source_id, dc_entry->source_id);
    EXPECT_FALSE(test_ukm_recorder.GetSourceForSourceId(dc_entry->source_id));
    EXPECT_EQ(main_url,
              test_ukm_recorder
                  .GetSourceForSourceId(*test_ukm_recorder.GetEntryMetric(
                      dc_entry, CreatedEntry::kNavigationSourceIdName))
                  ->url());
    EXPECT_EQ(0, *test_ukm_recorder.GetEntryMetric(
                     dc_entry, CreatedEntry::kIsMainFrameName));
  }

  // Navigate top frame, try play.
  NavigateFrameAndWait(web_contents()->GetPrimaryMainFrame(), foo_url);
  TryAutoplay(test_ukm_recorder, web_contents());

  // Check that we recorded a UKM event using the main frame URL.
  {
    auto ukm_entries = test_ukm_recorder.GetEntriesByName(Entry::kEntryName);

    ASSERT_EQ(4u, ukm_entries.size());
    test_ukm_recorder.ExpectEntrySourceHasUrl(ukm_entries[3], foo_url);
  }
}

}  // namespace
