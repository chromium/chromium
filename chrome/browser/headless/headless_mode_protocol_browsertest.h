// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PROTOCOL_BROWSERTEST_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PROTOCOL_BROWSERTEST_H_

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/headless/headless_mode_devtooled_browsertest.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "components/headless/test/test_meta_info.h"

namespace headless {

class HeadlessModeProtocolBrowserTest
    : public HeadlessModeDevTooledBrowserTest {
 public:
  HeadlessModeProtocolBrowserTest();
  ~HeadlessModeProtocolBrowserTest() override;

 protected:
  // Implement this to provide the relative script name;
  virtual std::string GetScriptName() = 0;

  // Implement this for tests that need to pass extra parameters to
  // JavaScript code.
  virtual base::Value::Dict GetPageUrlExtraParams();

  // Returns relative test data directory.
  base::FilePath GetTestDataDir();

  // Returns absolute script file path.
  base::FilePath GetScriptPath();

  // Returns absolute expectations file path.
  base::FilePath GetTestExpectationFilePath();

  bool IsSharedTestScript();

  void LoadTestMetaInfo();

  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  void StartEmbeddedTestServer();

  // HeadlessModeDevTooledBrowserTest:
  void RunDevTooledTest() override;

  void OnDevToolsProtocolExposed(base::Value::Dict params);
  void OnLoadEventFired(const base::Value::Dict& params);
  void OnEvaluateResult(base::Value::Dict params);

  void ProcessTestResult(const std::string& test_result);

  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  TestMetaInfo test_meta_info_;
};

#define HEADLESS_MODE_PROTOCOL_TEST(TEST_NAME, SCRIPT_NAME)           \
  class HeadlessModeProtocolBrowserTest_##TEST_NAME                   \
      : public HeadlessModeProtocolBrowserTest {                      \
   public:                                                            \
    std::string GetScriptName() override {                            \
      return SCRIPT_NAME;                                             \
    }                                                                 \
  };                                                                  \
                                                                      \
  IN_PROC_BROWSER_TEST_F(HeadlessModeProtocolBrowserTest_##TEST_NAME, \
                         TEST_NAME) {                                 \
    RunTest();                                                        \
  }

#define HEADLESS_MODE_PROTOCOL_TEST_F(TEST_FIXTURE, TEST_NAME, SCRIPT_NAME) \
  class TEST_FIXTURE##_##TEST_NAME : public TEST_FIXTURE {                  \
   public:                                                                  \
    std::string GetScriptName() override {                                  \
      return SCRIPT_NAME;                                                   \
    }                                                                       \
  };                                                                        \
                                                                            \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE##_##TEST_NAME, TEST_NAME) {           \
    RunTest();                                                              \
  }

#define HEADLESS_MODE_PROTOCOL_TEST_P(TEST_FIXTURE, TEST_NAME, SCRIPT_NAME) \
  class TEST_FIXTURE##_##TEST_NAME : public TEST_FIXTURE {                  \
   public:                                                                  \
    std::string GetScriptName() override {                                  \
      return SCRIPT_NAME;                                                   \
    }                                                                       \
  };                                                                        \
                                                                            \
  IN_PROC_BROWSER_TEST_P(TEST_FIXTURE##_##TEST_NAME, TEST_NAME) {           \
    RunTest();                                                              \
  }

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_PROTOCOL_BROWSERTEST_H_
