// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/config.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/chrome_for_testing/prefs.h"
#include "chrome/browser/chrome_for_testing/switches.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_for_testing {
namespace {

const char* kBooleanPrefs[] = {
    prefs::kEnableUserEducationUI,
    prefs::kEnableSearchEngineChoiceDialog,
    prefs::kEnableVirtualClipboard,
};

class ChromeForTestingConfigTest : public testing::Test {
 public:
  ChromeForTestingConfigTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterPrefs(pref_service_->registry());
  }

  void CreateConfig(std::string_view config_json) {
    base::FilePath config_path = temp_dir_.GetPath().AppendASCII("config.json");
    ASSERT_TRUE(base::WriteFile(config_path, config_json));

    scoped_command_line_.GetProcessCommandLine()->AppendSwitchPath(
        switches::kChromeForTestingConfig, config_path);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedCommandLine scoped_command_line_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(ChromeForTestingConfigTest, InvalidJson) {
  static constexpr char kJson[] = R"(
    {
      "key": fooBar
    }
  )";

  CreateConfig(kJson);

  ASSERT_FALSE(LoadConfig(pref_service_.get()));
}

TEST_F(ChromeForTestingConfigTest, EmptyJson) {
  static constexpr char kJson[] = R"(
    {
    }
  )";

  CreateConfig(kJson);

  ASSERT_TRUE(LoadConfig(pref_service_.get()));

  // Expect all options to be disabled by default.
  for (const char* pref : kBooleanPrefs) {
    EXPECT_FALSE(pref_service_->GetBoolean(pref));
  }
}

class ChromeForTestingConfigBooleanOptionTest
    : public ChromeForTestingConfigTest,
      public testing::WithParamInterface<std::tuple<std::string, std::string>> {
 protected:
  std::string json_key() { return std::get<0>(GetParam()); }
  std::string pref_name() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ChromeForTestingConfigBooleanOptionTest,
    testing::Values(std::make_tuple("enableUserEducationUI",
                                    prefs::kEnableUserEducationUI),
                    std::make_tuple("enableSearchEngineChoiceDialog",
                                    prefs::kEnableSearchEngineChoiceDialog),
                    std::make_tuple("enableVirtualClipboard",
                                    prefs::kEnableVirtualClipboard)),
    [](const testing::TestParamInfo<
        ChromeForTestingConfigBooleanOptionTest::ParamType>& info) {
      return std::get<0>(info.param);
    });

TEST_P(ChromeForTestingConfigBooleanOptionTest, OptionIsTrue) {
  static constexpr char kJson[] = R"(
    {
      "%s": true
    }
  )";
  std::string config_json = base::StringPrintf(kJson, json_key());

  CreateConfig(config_json);

  ASSERT_TRUE(LoadConfig(pref_service_.get()));

  // Expect only the current option to be enabled.
  for (const char* pref : kBooleanPrefs) {
    EXPECT_EQ(pref_service_->GetBoolean(pref), pref_name() == pref);
  }
}

}  // namespace
}  // namespace chrome_for_testing
