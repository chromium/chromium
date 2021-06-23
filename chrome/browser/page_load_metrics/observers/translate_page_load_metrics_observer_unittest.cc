// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/translate_page_load_metrics_observer.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/translate/core/browser/mock_translate_metrics_logger.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
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

  void LogAutofillAssistantDeferredTriggerDecision() override {
    mock_translate_metrics_logger_
        ->LogAutofillAssistantDeferredTriggerDecision();
  }

  void LogInitialState() override {
    mock_translate_metrics_logger_->LogInitialState();
  }

  void LogTranslationStarted(
      translate::TranslationType translation_type) override {
    mock_translate_metrics_logger_->LogTranslationStarted(translation_type);
  }

  void LogTranslationFinished(
      bool was_successful,
      translate::TranslateErrors::Type error_type) override {
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

  translate::TranslationType GetNextManualTranslationType() override {
    return mock_translate_metrics_logger_->GetNextManualTranslationType();
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

 private:
  translate::testing::MockTranslateMetricsLogger*
      mock_translate_metrics_logger_;  // Weak.
};

class TranslatePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  void SetUp() override {
    PageLoadMetricsObserverTestHarness::SetUp();

    // Creates the MockTranslateMetricsLogger that will be used for this test.
    mock_translate_metrics_logger_ =
        std::make_unique<translate::testing::MockTranslateMetricsLogger>();
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    translate::testing::MockTranslateMetricsLogger*
        raw_mock_translate_metrics_logger =
            mock_translate_metrics_logger_.get();

    // Wraps the raw pointer in a container.
    std::unique_ptr<MockTranslateMetricsLoggerContainer>
        mock_translate_metrics_logger_container =
            std::make_unique<MockTranslateMetricsLoggerContainer>(
                raw_mock_translate_metrics_logger);

    tracker->AddObserver(std::make_unique<TranslatePageLoadMetricsObserver>(
        std::move(mock_translate_metrics_logger_container)));
  }

  translate::testing::MockTranslateMetricsLogger&
  mock_translate_metrics_logger() const {
    return *mock_translate_metrics_logger_;
  }

 private:
  // This is the TranslateMetricsLoggers used in a test.It is owned by the
  // TranslatePageLoadMetricsObserverTest.
  std::unique_ptr<translate::testing::MockTranslateMetricsLogger>
      mock_translate_metrics_logger_;
};

TEST_F(TranslatePageLoadMetricsObserverTest, SinglePageLoad) {
  EXPECT_CALL(mock_translate_metrics_logger(), OnPageLoadStart(true)).Times(1);
  EXPECT_CALL(mock_translate_metrics_logger(), OnPageLoadStart(false)).Times(0);
  EXPECT_CALL(mock_translate_metrics_logger(), OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(mock_translate_metrics_logger(), RecordMetrics(true)).Times(1);
  EXPECT_CALL(mock_translate_metrics_logger(), RecordMetrics(false)).Times(0);

  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->NavigateToUntrackedUrl();
}

TEST_F(TranslatePageLoadMetricsObserverTest, AppEntersBackground) {
  EXPECT_CALL(mock_translate_metrics_logger(), OnPageLoadStart(true)).Times(1);
  EXPECT_CALL(mock_translate_metrics_logger(), OnPageLoadStart(false)).Times(0);
  EXPECT_CALL(mock_translate_metrics_logger(), OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(mock_translate_metrics_logger(), RecordMetrics(true)).Times(1);
  EXPECT_CALL(mock_translate_metrics_logger(), RecordMetrics(false)).Times(1);

  NavigateAndCommit(GURL("https://www.example.com"));
  tester()->SimulateAppEnterBackground();
  tester()->NavigateToUntrackedUrl();
}

TEST_F(TranslatePageLoadMetricsObserverTest, RepeatedAppEntersBackground) {
  int num_times_enter_background = 100;

  EXPECT_CALL(mock_translate_metrics_logger(), OnPageLoadStart(true)).Times(1);
  EXPECT_CALL(mock_translate_metrics_logger(), OnPageLoadStart(false)).Times(0);
  EXPECT_CALL(mock_translate_metrics_logger(), OnForegroundChange(testing::_))
      .Times(0);
  EXPECT_CALL(mock_translate_metrics_logger(), RecordMetrics(true)).Times(1);
  EXPECT_CALL(mock_translate_metrics_logger(), RecordMetrics(false))
      .Times(num_times_enter_background);

  NavigateAndCommit(GURL("https://www.example.com"));
  for (int i = 0; i < num_times_enter_background; ++i)
    tester()->SimulateAppEnterBackground();

  tester()->NavigateToUntrackedUrl();
}

// TODO(curranmax): Add unit tests that confirm behavior when the hidden/shown.
// status of the tab changes. https://crbug.com/1114868.
