// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/translate_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/translate/core/browser/mock_translate_metrics_logger.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

// Wraps a MockTranslateMetricsLogger so that test can retain a pointer to the
// MockTranslateMetricsLogger after the TranslatePageLoadMetricsObserver is done
// with it.
class MockTranslateMetricsLoggerContainer
    : public translate::TranslateMetricsLogger {
 public:
  explicit MockTranslateMetricsLoggerContainer(
      translate::testing::MockTranslateMetricsLogger*
          mock_translate_metrics_logger)
      : mock_translate_metrics_logger_(mock_translate_metrics_logger) {}

  void OnPageLoadStart(bool is_foreground) override {
    mock_translate_metrics_logger_->OnPageLoadStart(is_foreground);
  }

  void OnForegroundChange(bool is_foreground) override {
    mock_translate_metrics_logger_->OnForegroundChange(is_foreground);
  }

  void RecordMetrics(bool is_final) override {
    mock_translate_metrics_logger_->RecordMetrics(is_final);
  }

  void SetUkmSourceId(ukm::SourceId ukm_source_id) override {
    mock_translate_metrics_logger_->SetUkmSourceId(ukm_source_id);
  }

  void LogRankerMetrics(translate::RankerDecision ranker_decision,
                        uint32_t ranker_version) override {
    mock_translate_metrics_logger_->LogRankerMetrics(ranker_decision,
                                                     ranker_version);
  }

  void LogRankerStart() override {
    mock_translate_metrics_logger_->LogRankerStart();
  }

  void LogRankerFinish() override {
    mock_translate_metrics_logger_->LogRankerFinish();
  }

  void LogTriggerDecision(
      translate::TriggerDecision trigger_decision) override {
    mock_translate_metrics_logger_->LogTriggerDecision(trigger_decision);
  }

  void LogInitialState() override {
    mock_translate_metrics_logger_->LogInitialState();
  }

  void LogTranslationStarted(
      translate::TranslationType translation_type) override {
    mock_translate_metrics_logger_->LogTranslationStarted(translation_type);
  }

  void LogTranslationFinished(bool was_successful,
                              translate::TranslateErrors error_type) override {
    mock_translate_metrics_logger_->LogTranslationFinished(was_successful,
                                                           error_type);
  }

  void LogReversion() override {
    mock_translate_metrics_logger_->LogReversion();
  }

  void LogUIChange(bool is_ui_shown) override {
    mock_translate_metrics_logger_->LogUIChange(is_ui_shown);
  }

  void LogOmniboxIconChange(bool is_omnibox_icon_shown) override {
    mock_translate_metrics_logger_->LogOmniboxIconChange(is_omnibox_icon_shown);
  }

  void LogInitialSourceLanguage(const std::string& source_language_code,
                                bool is_in_users_content_languages) override {
    mock_translate_metrics_logger_->LogInitialSourceLanguage(
        source_language_code, is_in_users_content_languages);
  }

  void LogSourceLanguage(const std::string& source_language_code) override {
    mock_translate_metrics_logger_->LogSourceLanguage(source_language_code);
  }

  void LogTargetLanguage(
      const std::string& target_language_code,
      translate::TranslateBrowserMetrics::TargetLanguageOrigin
          target_language_origin) override {
    mock_translate_metrics_logger_->LogTargetLanguage(target_language_code,
                                                      target_language_origin);
  }

  void LogUIInteraction(translate::UIInteraction ui_interaction) override {
    mock_translate_metrics_logger_->LogUIInteraction(ui_interaction);
  }

  translate::TranslationType GetNextManualTranslationType(
      bool is_context_menu_initiated_translation) override {
    return mock_translate_metrics_logger_->GetNextManualTranslationType(
        is_context_menu_initiated_translation);
  }

  void SetHasHrefTranslateTarget(bool has_href_translate_target) override {
    mock_translate_metrics_logger_->SetHasHrefTranslateTarget(
        has_href_translate_target);
  }

  void LogHTMLDocumentLanguage(const std::string& html_doc_language) override {
    mock_translate_metrics_logger_->LogHTMLDocumentLanguage(html_doc_language);
  }

  void LogHTMLContentLanguage(
      const std::string& html_content_language) override {
    mock_translate_metrics_logger_->LogHTMLDocumentLanguage(
        html_content_language);
  }

  void LogDetectedLanguage(const std::string& detected_language) override {
    mock_translate_metrics_logger_->LogDetectedLanguage(detected_language);
  }

  void LogDetectionReliabilityScore(
      const float& model_detection_reliability_score) override {
    mock_translate_metrics_logger_->LogDetectionReliabilityScore(
        model_detection_reliability_score);
  }

  void LogWasContentEmpty(bool was_content_empty) override {
    mock_translate_metrics_logger_->LogWasContentEmpty(was_content_empty);
  }

 private:
  raw_ptr<translate::testing::MockTranslateMetricsLogger>
      mock_translate_metrics_logger_;  // Weak.
};

class TranslatePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void PrepareMock(size_t n = 1) {
    for (size_t i = 0; i < n; ++i) {
      // Creates the MockTranslateMetricsLogger that will be used for this test.
      mock_translate_metrics_loggers_.emplace_back(
          std::make_unique<translate::testing::MockTranslateMetricsLogger>());
    }
  }
  const std::vector<
      std::unique_ptr<translate::testing::MockTranslateMetricsLogger>>&
  mock_translate_metrics_loggers() const {
    return mock_translate_metrics_loggers_;
  }

 private:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    EXPECT_LT(used_mock_count_, mock_translate_metrics_loggers_.size());
    translate::testing::MockTranslateMetricsLogger*
        raw_mock_translate_metrics_logger =
            mock_translate_metrics_loggers_[used_mock_count_++].get();

    // Wraps the raw pointer in a container.
    std::unique_ptr<MockTranslateMetricsLoggerContainer>
        mock_translate_metrics_logger_container =
            std::make_unique<MockTranslateMetricsLoggerContainer>(
                raw_mock_translate_metrics_logger);

    tracker->AddObserver(std::make_unique<TranslatePageLoadMetricsObserver>(
        std::move(mock_translate_metrics_logger_container)));
  }

  // This is the TranslateMetricsLoggers used in a test.It is owned by the
  // TranslatePageLoadMetricsObserverTest.
  std::vector<std::unique_ptr<translate::testing::MockTranslateMetricsLogger>>
      mock_translate_metrics_loggers_;
  size_t used_mock_count_ = 0u;
};

TEST_F(TranslatePageLoadMetricsObserverTest, SinglePageLoad) {
  PrepareMock();

  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(false))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0],
              OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], SetUkmSourceId(testing::_))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(false))
      .Times(0);

  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->NavigateToUntrackedUrl();
}

TEST_F(TranslatePageLoadMetricsObserverTest, AppEntersBackground) {
  PrepareMock();

  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(false))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0],
              OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], SetUkmSourceId(testing::_))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(false))
      .Times(1);

  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->SimulateAppEnterBackground();
  tester()->NavigateToUntrackedUrl();
}

TEST_F(TranslatePageLoadMetricsObserverTest, RepeatedAppEntersBackground) {
  PrepareMock();

  int num_times_enter_background = 100;

  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(false))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0],
              OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], SetUkmSourceId(testing::_))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(false))
      .Times(num_times_enter_background);

  NavigateAndCommit(GURL("https://www.example.com"));
  for (int i = 0; i < num_times_enter_background; ++i)
    tester()->SimulateAppEnterBackground();

  tester()->NavigateToUntrackedUrl();
}

TEST_F(TranslatePageLoadMetricsObserverTest, PrerenderAndActivation) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());

  PrepareMock(2);

  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], OnPageLoadStart(false))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0],
              OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], SetUkmSourceId(testing::_))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[0], RecordMetrics(false))
      .Times(0);

  EXPECT_CALL(*mock_translate_metrics_loggers()[1], OnPageLoadStart(true))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[1], OnPageLoadStart(false))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[1],
              OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(*mock_translate_metrics_loggers()[1], SetUkmSourceId(testing::_))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[1], RecordMetrics(true))
      .Times(1);
  EXPECT_CALL(*mock_translate_metrics_loggers()[1], RecordMetrics(false))
      .Times(0);

  // Navigate to the initial page to set the initiator page's origin explicitly.
  NavigateAndCommit(GURL("https://www.example.com"));

  const GURL kPrerenderingUrl("https://www.example.com/prerender");
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(kPrerenderingUrl);

  // Activation
  content::WebContentsTester::For(web_contents())
      ->ActivatePrerenderedPage(kPrerenderingUrl);

  tester()->NavigateToUntrackedUrl();
}

// TODO(curranmax): Add unit tests that confirm behavior when the hidden/shown.
// status of the tab changes. https://crbug.com/1114868.
