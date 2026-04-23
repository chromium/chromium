// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_for_testing/config.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_for_testing/prefs.h"
#include "chrome/browser/chrome_for_testing/switches.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/prefs/pref_service.h"
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
    pref_service_ = g_browser_process->local_state();
    CHECK(pref_service_);
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
  raw_ptr<PrefService> pref_service_;
};

TEST_F(ChromeForTestingConfigTest, InvalidJson) {
  static constexpr char kJson[] = R"(
    {
      "key": fooBar
    }
  )";

  CreateConfig(kJson);

  ASSERT_FALSE(LoadConfig(pref_service_));
}

TEST_F(ChromeForTestingConfigTest, EmptyJson) {
  static constexpr char kJson[] = R"(
    {
    }
  )";

  CreateConfig(kJson);

  ASSERT_TRUE(LoadConfig(pref_service_));

  // Expect all boolean options to be disabled by default.
  for (const char* pref : kBooleanPrefs) {
    EXPECT_FALSE(pref_service_->GetBoolean(pref));
  }

  // Expect required components list to be empty.
  EXPECT_TRUE(GetRequiredComponentsList().empty());

  // Expect default required components update timeout.
  EXPECT_EQ(GetRequiredComponentsUpdateTimeout(), base::Seconds(15));
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

  ASSERT_TRUE(LoadConfig(pref_service_));

  // Expect only the current option to be enabled.
  for (const char* pref : kBooleanPrefs) {
    EXPECT_EQ(pref_service_->GetBoolean(pref), pref_name() == pref);
  }
}

TEST_F(ChromeForTestingConfigTest, RequiredComponentsList) {
  static constexpr char kJson[] = R"(
    {
      "requiredComponents": [
        {"name": "Hyphenation"},
        {"name": "FooBar", "version": "1.2.3.4"},
        {"name": "Zulu One", "version": ""},
      ],
    }
  )";

  CreateConfig(kJson);

  ASSERT_TRUE(LoadConfig(pref_service_));

  base::flat_map<std::string, std::string> kExpected(
      {{"FooBar", "1.2.3.4"}, {"Hyphenation", ""}, {"Zulu One", ""}});

  EXPECT_EQ(GetRequiredComponentsMap(), kExpected);
}

TEST_F(ChromeForTestingConfigTest, InvalidRequiredComponentsListType) {
  static constexpr char kJson[] = R"(
    {
      "requiredComponents": {"name": "Hyphenation"},
    }
  )";

  CreateConfig(kJson);

  EXPECT_FALSE(LoadConfig(pref_service_));
}

TEST_F(ChromeForTestingConfigTest, InvalidRequiredComponentsListComponentName) {
  {
    static constexpr char kJson[] = R"(
      {
        "requiredComponents": [
          {"name": ""},
        ],
      }
    )";

    CreateConfig(kJson);

    EXPECT_FALSE(LoadConfig(pref_service_));
  }

  {
    static constexpr char kJson[] = R"(
      {
        "requiredComponents": [
          {"name": 42},
        ],
      }
    )";

    CreateConfig(kJson);

    EXPECT_FALSE(LoadConfig(pref_service_));
  }

  {
    static constexpr char kJson[] = R"(
      {
        "requiredComponents": [
          {"noName": "foobar"},
        ],
      }
    )";

    CreateConfig(kJson);

    EXPECT_FALSE(LoadConfig(pref_service_));
  }
}

TEST_F(ChromeForTestingConfigTest,
       InvalidRequiredComponentsListComponentVersion) {
  {
    static constexpr char kJson[] = R"(
      {
        "requiredComponents": [
          {"name": "FooBar", "version": "1-2-3-4"},
        ],
      }
    )";

    CreateConfig(kJson);

    EXPECT_FALSE(LoadConfig(pref_service_));
  }

  {
    static constexpr char kJson[] = R"(
      {
        "requiredComponents": [
          {"name": "FooBar", "version": 123},
        ],
      }
    )";

    CreateConfig(kJson);

    EXPECT_FALSE(LoadConfig(pref_service_));
  }
}

constexpr base::TimeDelta kInvalidTimeout = base::Seconds(0);

class ChromeForTestingConfigRequiredComponentsUpdateTimeoutTest
    : public ChromeForTestingConfigTest,
      public testing::WithParamInterface<
          std::tuple<std::string, base::TimeDelta>> {
 protected:
  std::string json_value() { return std::get<0>(GetParam()); }
  base::TimeDelta expected_timeout() { return std::get<1>(GetParam()); }
  bool expect_successful_load() {
    return expected_timeout() != kInvalidTimeout;
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ChromeForTestingConfigRequiredComponentsUpdateTimeoutTest,
    testing::Values(std::make_tuple("10s", base::Seconds(10)),
                    std::make_tuple("1000ms", base::Milliseconds(1000)),
                    std::make_tuple("500ms", kInvalidTimeout),  // too small
                    std::make_tuple("10m", kInvalidTimeout),    // too large
                    std::make_tuple("foobar", kInvalidTimeout)),
    [](const testing::TestParamInfo<
        ChromeForTestingConfigRequiredComponentsUpdateTimeoutTest::ParamType>&
           info) { return std::get<0>(info.param); });

TEST_P(ChromeForTestingConfigRequiredComponentsUpdateTimeoutTest,
       RequiredComponentsUpdateTimeout) {
  static constexpr char kJson[] = R"(
    {
      "requiredComponentsUpdateTimeout": "%s"
    }
  )";
  std::string config_json = base::StringPrintf(kJson, json_value());

  CreateConfig(config_json);

  ASSERT_EQ(LoadConfig(pref_service_), expect_successful_load());

  if (expect_successful_load()) {
    EXPECT_EQ(GetRequiredComponentsUpdateTimeout(), expected_timeout());
  }
}

TEST_F(ChromeForTestingConfigTest, RequiredComponentsDir) {
  static constexpr char kJson[] = R"(
    {
      "requiredComponentsDir": "/tmp/components"
    }
  )";

  CreateConfig(kJson);

  ASSERT_TRUE(LoadConfig(pref_service_));

  base::FilePath expected_path(FILE_PATH_LITERAL("/tmp/components"));
  EXPECT_EQ(GetRequiredComponentsDir(), expected_path);

  // Verify the path override. Note that use of base::MakeAbsoluteFilePath() is
  // important as it resolves symlinks (e.g. /tmp -> /private/tmp on macOS).
  base::FilePath path;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER, &path);
  EXPECT_EQ(base::MakeAbsoluteFilePath(path),
            base::MakeAbsoluteFilePath(expected_path));
}

TEST_F(ChromeForTestingConfigTest, InvalidRequiredComponentsDir) {
  static constexpr char kJson[] = R"(
    {
      "requiredComponentsDir": ""
    }
  )";

  CreateConfig(kJson);

  EXPECT_FALSE(LoadConfig(pref_service_));
}

TEST_F(ChromeForTestingConfigTest, UnknownConfigKeyword) {
  static constexpr char kJson[] = R"(
    {
      "unknownKeyword": "value"
    }
  )";

  CreateConfig(kJson);

  EXPECT_FALSE(LoadConfig(pref_service_));
}

TEST_F(ChromeForTestingConfigTest, ConfigSwitchWithNoValue) {
  // Alter default config.
  pref_service_->SetBoolean(prefs::kEnableUserEducationUI, true);
  pref_service_->SetBoolean(prefs::kEnableSearchEngineChoiceDialog, true);
  pref_service_->SetBoolean(prefs::kEnableVirtualClipboard, true);

  base::ListValue required_components;
  required_components.Append(base::DictValue().Set("name", "*"));
  pref_service_->SetList(prefs::kRequiredComponents,
                         required_components.Clone());

  base::FilePath required_components_dir(FILE_PATH_LITERAL("/tmp/components"));
  pref_service_->SetFilePath(prefs::kRequiredComponentsDir,
                             required_components_dir);

  base::TimeDelta required_components_update_timeout = base::Seconds(42);
  pref_service_->SetTimeDelta(prefs::kRequiredComponentsUpdateTimeout,
                              required_components_update_timeout);

  // Append the --chrome-for-testing-config switch with no path specification so
  // that the loaded configuration will be preserved.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
      switches::kChromeForTestingConfig);

  ASSERT_TRUE(LoadConfig(pref_service_));

  // Verify that altered configuration persistes.
  EXPECT_TRUE(IsEnableUserEducationUI());
  EXPECT_TRUE(IsEnableSearchEngineChoiceDialog());
  EXPECT_TRUE(IsEnableVirtualClipboard());

  EXPECT_EQ(GetRequiredComponentsList(), required_components);
  EXPECT_EQ(GetRequiredComponentsDir(), required_components_dir);
  EXPECT_EQ(GetRequiredComponentsUpdateTimeout(),
            required_components_update_timeout);

  base::FilePath path;
  base::PathService::Get(component_updater::DIR_COMPONENT_USER, &path);
  EXPECT_EQ(base::MakeAbsoluteFilePath(path),
            base::MakeAbsoluteFilePath(required_components_dir));
}

TEST_F(ChromeForTestingConfigTest, NoConfigSwitch) {
  // Alter default config.
  pref_service_->SetBoolean(prefs::kEnableUserEducationUI, true);
  pref_service_->SetBoolean(prefs::kEnableSearchEngineChoiceDialog, true);
  pref_service_->SetBoolean(prefs::kEnableVirtualClipboard, true);

  base::ListValue required_components;
  required_components.Append(base::DictValue().Set("name", "*"));
  pref_service_->SetList(prefs::kRequiredComponents,
                         required_components.Clone());

  base::FilePath required_components_dir(FILE_PATH_LITERAL("/tmp/components"));
  pref_service_->SetFilePath(prefs::kRequiredComponentsDir,
                             required_components_dir);

  base::TimeDelta required_components_update_timeout = base::Seconds(42);
  pref_service_->SetTimeDelta(prefs::kRequiredComponentsUpdateTimeout,
                              required_components_update_timeout);

  // Loading config with no --chrome-for-testing-config switch should reset
  // configuration to default.
  ASSERT_TRUE(LoadConfig(pref_service_));

  EXPECT_FALSE(IsEnableUserEducationUI());
  EXPECT_FALSE(IsEnableSearchEngineChoiceDialog());
  EXPECT_FALSE(IsEnableVirtualClipboard());

  EXPECT_EQ(GetRequiredComponentsList(), base::ListValue());
  EXPECT_EQ(GetRequiredComponentsDir(), base::FilePath());
  EXPECT_EQ(GetRequiredComponentsUpdateTimeout(), base::Seconds(15));
}

}  // namespace
}  // namespace chrome_for_testing
