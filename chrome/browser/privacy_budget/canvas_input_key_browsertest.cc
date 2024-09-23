// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

using base::StringToInt64;
using base::StringToUint64;
using blink::IdentifiableSurface;

constexpr char kFingerprintingScriptUrlSwitch[] = "fingerprinting-script-url";
constexpr char kFingerprintExpectationSwitch[] = "fingerprint-expectation";
constexpr char kInputKeyExpectationSwitch[] = "input-key-expectation";
constexpr char kValueExpectationSwitch[] = "value-expectation";

// NOTE: This test is *disabled* so that it doesn't run on waterfall -- to run
// the test, invoke the test binary as follows:
//
// testing/xvfb.py out/Default/browser_tests --gtest_also_run_disabled_tests
// --gtest_filter="*CanvasInputKeyBrowserTest*"
// --fingerprinting-script-url="file URL goes here"
// [--fingerprint-expectation="optional expected fingerprint goes here"]
// [--input-key-expectation="optional key expectation goes here"]
// [--value-expectation="optional value expectation goes here"]
//
// The --fingerprinting-script-url must resolve to an HTML page that runs a
// script that calls window.domAutomationController.send() with the computed
// fingerprint.
//
// This test runs on Android as well as desktop platforms.
class DISABLED_CanvasInputKeyBrowserTest : public PlatformBrowserTest {
 public:
  DISABLED_CanvasInputKeyBrowserTest() {
    privacy_budget_config_.Apply(test::ScopedPrivacyBudgetConfig::Parameters(
        test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling));
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    fingerprinting_script_url_ =
        command_line->GetSwitchValueASCII(kFingerprintingScriptUrlSwitch);
    fingerprint_expectation_ =
        command_line->GetSwitchValueASCII(kFingerprintExpectationSwitch);
    input_key_expectation_ =
        command_line->GetSwitchValueASCII(kInputKeyExpectationSwitch);
    value_expectation_ =
        command_line->GetSwitchValueASCII(kValueExpectationSwitch);
  }

  void SetUpOnMainThread() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

 protected:
  std::string fingerprinting_script_url_;
  std::string fingerprint_expectation_;
  std::string input_key_expectation_;
  std::string value_expectation_;

  test::ScopedPrivacyBudgetConfig privacy_budget_config_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

struct MetricKeyValue {
  uint64_t input_key;
  int64_t value;
};

// Verify that there's only one entry of type |type|, and return the the
// |input_key|, |value| pair.
template <typename MapType>
std::optional<MetricKeyValue> ExtractKeyOfType(IdentifiableSurface::Type type,
                                               const MapType& metrics) {
  MetricKeyValue last_result = {};
  for (const auto& pair : metrics) {
    auto surface = IdentifiableSurface::FromMetricHash(pair.first);
    if (surface.GetType() == type) {
      if (last_result.input_key != 0) {
        ADD_FAILURE() << "Saw at least 2 surfaces of type "
                      << static_cast<uint64_t>(type)
                      << ". First input hash: " << last_result.input_key
                      << " second input hash: " << surface.GetInputHash();
        return std::nullopt;
      }
      last_result.input_key = surface.GetInputHash();
      last_result.value = pair.second;
    }
  }
  return last_result;
}

IN_PROC_BROWSER_TEST_F(DISABLED_CanvasInputKeyBrowserTest,
                       TestCanvasFingerprint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::DOMMessageQueue messages(web_contents());
  base::RunLoop run_loop;

  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   run_loop.QuitClosure());

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(fingerprinting_script_url_)));

  // The document computes the canvas fingerprint and sends a message back to
  // the test. Receipt of the message indicates that the script successfully
  // completed.
  std::string fingerprint;
  ASSERT_TRUE(messages.WaitForMessage(&fingerprint));
  // Navigating away from the test page causes the document to be unloaded. That
  // will cause any buffered metrics to be flushed.
  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                      GURL("about:blank"), 1);
  // Wait for the metrics to come down the pipe.
  content::RunAllTasksUntilIdle();
  run_loop.Run();
  auto merged_entries = recorder().GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);
  // Shouldn't be more than one source here. If this changes, then we'd need to
  // adjust this test to deal.
  ASSERT_EQ(1u, merged_entries.size());

  std::optional<MetricKeyValue> canvas_key_value =
      ExtractKeyOfType(IdentifiableSurface::Type::kCanvasReadback,
                       merged_entries.begin()->second->metrics);
  ASSERT_TRUE(canvas_key_value);

  LOG(INFO) << "Canvas fingerprint is: " << fingerprint;
  LOG(INFO) << "Input key is: " << canvas_key_value->input_key;
  LOG(INFO) << "Value is: " << canvas_key_value->value;

  if (!fingerprint_expectation_.empty())
    EXPECT_EQ(fingerprint_expectation_, fingerprint);
  if (!input_key_expectation_.empty()) {
    uint64_t parsed_input_key_expectation;
    EXPECT_TRUE(
        StringToUint64(input_key_expectation_, &parsed_input_key_expectation));
    EXPECT_EQ(parsed_input_key_expectation, canvas_key_value->input_key);
  }
  if (!value_expectation_.empty()) {
    int64_t parsed_value_expectation;
    EXPECT_TRUE(StringToInt64(value_expectation_, &parsed_value_expectation));
    EXPECT_EQ(parsed_value_expectation, canvas_key_value->value);
  }
}

}  // namespace
