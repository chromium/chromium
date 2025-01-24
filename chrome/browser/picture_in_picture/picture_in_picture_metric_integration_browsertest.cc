// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/picture_in_picture/document_picture_in_picture_mixin_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"

namespace {

constexpr char kBlinkUseCounterHistogram[] = "Blink.UseCounter.Features";

constexpr char kHost[] = "a.com";

constexpr char kAutoDocumentPipPage[] =
    "/media/picture-in-picture/autopip-document.html";

constexpr char kDocumentPipPage[] =
    "/media/picture-in-picture/document-pip.html";

constexpr char kVideoConferencingPage[] =
    "/media/picture-in-picture/video-conferencing.html";

class PictureInPictureMetricIntegrationTest
    : public MixinBasedInProcessBrowserTest {
 public:
  PictureInPictureMetricIntegrationTest() = default;

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "chrome/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DocumentPictureInPictureMixinTestBase& picture_in_picture_test_base() {
    return picture_in_picture_test_base_;
  }

  bool RecordedMetricForBlinkCounterUKMEntry(
      const blink::mojom::WebFeature& expected_entry,
      const GURL& expected_url) {
    const auto& entries = ukm_recorder()->GetEntriesByName(
        ukm::builders::Blink_UseCounter::kEntryName);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src->url() != expected_url) {
        continue;
      }

      const int64_t* metric = ukm_recorder()->GetEntryMetric(
          entry, ukm::builders::Blink_UseCounter::kFeatureName);
      if (*metric == static_cast<int>(expected_entry)) {
        return true;
      }
    }

    return false;
  }

  void ExpectBlinkCounterUMABucketCount(const blink::mojom::WebFeature& sample,
                                        base::HistogramBase::Count32 count) {
    histogram_tester()->ExpectBucketCount(kBlinkUseCounterHistogram, sample,
                                          count);
  }

 private:
  DocumentPictureInPictureMixinTestBase picture_in_picture_test_base_{
      &mixin_host_};
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(PictureInPictureMetricIntegrationTest,
                       MetricsRecorded_MediaSessionEnterPictureInPicture) {
  // Navigate to a page that registers for the enterpictureinpicture action
  // handler.
  const auto& test_page_url =
      embedded_https_test_server().GetURL(kHost, kAutoDocumentPipPage);
  picture_in_picture_test_base().NavigateToUrl(browser(), test_page_url);
  picture_in_picture_test_base().WaitForPageLoad(web_contents());

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Verify that a UKM metric entry for Blink_UseCounter was recorded.
  ASSERT_TRUE(RecordedMetricForBlinkCounterUKMEntry(
      blink::mojom::WebFeature::kMediaSessionEnterPictureInPicture,
      test_page_url));

  // Verify the expected Blink_UseCounter UMA bucket count.
  ExpectBlinkCounterUMABucketCount(
      blink::mojom::WebFeature::kMediaSessionEnterPictureInPicture, 1);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureMetricIntegrationTest,
                       MetricsNotRecorded_MediaSessionEnterPictureInPicture) {
  // Navigate to a page that does not register for the enterpictureinpicture
  // action handler.
  const auto& test_page_url =
      embedded_https_test_server().GetURL(kHost, kDocumentPipPage);
  picture_in_picture_test_base().NavigateToUrl(browser(), test_page_url);
  picture_in_picture_test_base().WaitForPageLoad(web_contents());

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Verify that a UKM metric entry for Blink_UseCounter was not recorded.
  ASSERT_FALSE(RecordedMetricForBlinkCounterUKMEntry(
      blink::mojom::WebFeature::kMediaSessionEnterPictureInPicture,
      test_page_url));

  // Verify the expected Blink_UseCounter UMA bucket count.
  ExpectBlinkCounterUMABucketCount(
      blink::mojom::WebFeature::kMediaSessionEnterPictureInPicture, 0);
}

IN_PROC_BROWSER_TEST_F(
    PictureInPictureMetricIntegrationTest,
    MetricsNotRecorded_MediaSessionEnterPictureInPictureRandomHandler) {
  // Navigate to a page that registers various media session action handlers,
  // except for enterpictureinpicture.
  const auto& test_page_url =
      embedded_https_test_server().GetURL(kHost, kVideoConferencingPage);
  picture_in_picture_test_base().NavigateToUrl(browser(), test_page_url);
  picture_in_picture_test_base().WaitForPageLoad(web_contents());

  // Close browser to trigger metric recording.
  CloseBrowserSynchronously(browser());

  // Verify that a UKM metric entry for Blink_UseCounter was not recorded.
  ASSERT_FALSE(RecordedMetricForBlinkCounterUKMEntry(
      blink::mojom::WebFeature::kMediaSessionEnterPictureInPicture,
      test_page_url));

  // Verify the expected Blink_UseCounter UMA bucket count.
  ExpectBlinkCounterUMABucketCount(
      blink::mojom::WebFeature::kMediaSessionEnterPictureInPicture, 0);
}

}  // namespace
