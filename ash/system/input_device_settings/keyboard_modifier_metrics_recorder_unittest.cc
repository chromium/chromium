// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/keyboard_modifier_metrics_recorder.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ash/pref_names.h"

namespace ash {

namespace {
constexpr char kUserEmail1[] = "example1@abc.com";
constexpr char kUserEmail2[] = "example2@abc.com";

// Table containing the list of modifier remapping prefs with their expected
// metric names and their matching default key. Used by the following test
// suites.
struct KeyboardModifierMetricsRecorderTestData {
  std::string pref_name;
  std::string changed_metric_name;
  std::string started_metric_name;
  ui::mojom::ModifierKey default_modifier_key;
} kKeyboardModifierMetricTestData[] = {
    {prefs::kLanguageRemapAltKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.AltRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.AltRemappedTo.Started",
     ui::mojom::ModifierKey::kAlt},
    {prefs::kLanguageRemapControlKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.ControlRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.ControlRemappedTo.Started",
     ui::mojom::ModifierKey::kControl},
    {prefs::kLanguageRemapEscapeKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.EscapeRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.EscapeRemappedTo.Started",
     ui::mojom::ModifierKey::kEscape},
    {prefs::kLanguageRemapBackspaceKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.BackspaceRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.BackspaceRemappedTo.Started",
     ui::mojom::ModifierKey::kBackspace},
    {prefs::kLanguageRemapAssistantKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.AssistantRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.AssistantRemappedTo.Started",
     ui::mojom::ModifierKey::kAssistant},
    {prefs::kLanguageRemapCapsLockKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.CapsLockRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.CapsLockRemappedTo.Started",
     ui::mojom::ModifierKey::kCapsLock},
    {prefs::kLanguageRemapSearchKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.SearchRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.SearchRemappedTo.Started",
     ui::mojom::ModifierKey::kMeta},
    {prefs::kLanguageRemapExternalMetaKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.ExternalMetaRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.ExternalMetaRemappedTo.Started",
     ui::mojom::ModifierKey::kMeta},
    {prefs::kLanguageRemapExternalCommandKeyTo,
     "ChromeOS.Settings.Keyboard.Modifiers.ExternalCommandRemappedTo.Changed",
     "ChromeOS.Settings.Keyboard.Modifiers.ExternalCommandRemappedTo.Started",
     ui::mojom::ModifierKey::kControl},
};
}  // namespace

class KeyboardModifierMetricsRecorderTest : public AshTestBase {
 public:
  KeyboardModifierMetricsRecorderTest() = default;
  KeyboardModifierMetricsRecorderTest(
      const KeyboardModifierMetricsRecorderTest&) = delete;
  KeyboardModifierMetricsRecorderTest& operator=(
      const KeyboardModifierMetricsRecorderTest&) = delete;
  ~KeyboardModifierMetricsRecorderTest() override = default;

  void SetUp() override {
    feature_list_.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
    AshTestBase::SetUp();
    ResetHistogramTester();
    recorder_ = Shell::Get()->keyboard_modifier_metrics_recorder();
  }

  void TearDown() override {
    histogram_tester_.reset();
    AshTestBase::TearDown();
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  raw_ptr<KeyboardModifierMetricsRecorder, DanglingUntriaged> recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

class KeyboardModifierMetricsRecorderPrefChangedTest
    : public KeyboardModifierMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<KeyboardModifierMetricsRecorderTestData, int, int>> {
  void SetUp() override {
    KeyboardModifierMetricsRecorderTest::SetUp();
    int int_modifier_key_from, int_modifier_key_to;
    std::tie(data_, int_modifier_key_from, int_modifier_key_to) = GetParam();
    modifier_key_from_ =
        static_cast<ui::mojom::ModifierKey>(int_modifier_key_from);
    modifier_key_to_ = static_cast<ui::mojom::ModifierKey>(int_modifier_key_to);

    pref_service_ = Shell::Get()->session_controller()->GetActivePrefService();
    pref_service_->SetInteger(data_.pref_name,
                              static_cast<int>(data_.default_modifier_key));
    ResetHistogramTester();
  }

 protected:
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;

  KeyboardModifierMetricsRecorderTestData data_;
  ui::mojom::ModifierKey modifier_key_from_;
  ui::mojom::ModifierKey modifier_key_to_;
};

// Instantiates the test case with every combination of the modifiers in
// `kKeyboardModifierMetricTestData` and every combination of both a modifier
// to start with and a modifier to change to. Custom name generator is
// implemented to simplify searching through test results for failed cases.
INSTANTIATE_TEST_SUITE_P(
    ,
    KeyboardModifierMetricsRecorderPrefChangedTest,
    testing::Combine(
        testing::ValuesIn(kKeyboardModifierMetricTestData),
        testing::Range(static_cast<int>(ui::mojom::ModifierKey::kMinValue),
                       static_cast<int>(ui::mojom::ModifierKey::kAssistant) +
                           1),
        testing::Range(static_cast<int>(ui::mojom::ModifierKey::kMinValue),
                       static_cast<int>(ui::mojom::ModifierKey::kAssistant) +
                           1)),
    ([](const testing::TestParamInfo<
         KeyboardModifierMetricsRecorderPrefChangedTest::ParamType>& info) {
      const auto& [data, int_modifier_key_from, int_modifier_key_to] =
          info.param;
      // Pref name must replace periods with underscores for gtest output.
      std::string result;
      base::ReplaceChars(data.pref_name, ".", "_", &result);
      return base::StringPrintf("%s_%d_%d", result.c_str(),
                                int_modifier_key_from, int_modifier_key_to);
    }));

TEST_P(KeyboardModifierMetricsRecorderPrefChangedTest, CheckChangedMetric) {
  pref_service_->SetInteger(data_.pref_name,
                            static_cast<int>(modifier_key_from_));
  if (modifier_key_from_ != data_.default_modifier_key) {
    histogram_tester_->ExpectUniqueSample(data_.changed_metric_name,
                                          modifier_key_from_, 1);
  } else {
    histogram_tester_->ExpectUniqueSample(data_.changed_metric_name,
                                          modifier_key_from_, 0);
  }

  ResetHistogramTester();
  pref_service_->SetInteger(data_.pref_name,
                            static_cast<int>(modifier_key_to_));
  if (modifier_key_from_ != modifier_key_to_) {
    histogram_tester_->ExpectUniqueSample(data_.changed_metric_name,
                                          modifier_key_to_, 1);
  } else {
    histogram_tester_->ExpectUniqueSample(data_.changed_metric_name,
                                          modifier_key_to_, 0);
  }
}

class KeyboardModifierMetricsRecorderPrefStartedTest
    : public KeyboardModifierMetricsRecorderTest,
      public testing::WithParamInterface<
          std::tuple<KeyboardModifierMetricsRecorderTestData, int>> {
  void SetUp() override {
    KeyboardModifierMetricsRecorderTest::SetUp();
    int int_modifier_key;
    std::tie(data_, int_modifier_key) = GetParam();
    modifier_key_ = static_cast<ui::mojom::ModifierKey>(int_modifier_key);
    ResetHistogramTester();
  }

 protected:
  KeyboardModifierMetricsRecorderTestData data_;
  ui::mojom::ModifierKey modifier_key_;
};

// Instantiates the test case with every combination of the modifiers in
// `kKeyboardModifierMetricTestData` and with every possible remapped value in
// `ui::mojom::ModifierKey`. A custom name generator is implemented to simplify
// searching through test results for failed cases.
INSTANTIATE_TEST_SUITE_P(
    ,
    KeyboardModifierMetricsRecorderPrefStartedTest,
    testing::Combine(
        testing::ValuesIn(kKeyboardModifierMetricTestData),
        // Only test from 0 to Assistant as those are the only modifiers
        // supported pre settings split.
        testing::Range(static_cast<int>(ui::mojom::ModifierKey::kMinValue),
                       static_cast<int>(ui::mojom::ModifierKey::kAssistant) +
                           1)),
    ([](const testing::TestParamInfo<
         KeyboardModifierMetricsRecorderPrefStartedTest::ParamType>& info) {
      const auto& [data, int_modifier_key] = info.param;
      // Pref name must replace periods with underscores for gtest output.
      std::string result;
      base::ReplaceChars(data.pref_name, ".", "_", &result);
      return base::StringPrintf("%s_%d", result.c_str(), int_modifier_key);
    }));

TEST_P(KeyboardModifierMetricsRecorderPrefStartedTest, InitializeTest) {
  // Initialize two pref services with the initial value of the metric.
  const AccountId account_id1 = AccountId::FromUserEmail(kUserEmail1);
  const AccountId account_id2 = AccountId::FromUserEmail(kUserEmail2);

  std::unique_ptr<TestingPrefServiceSimple> pref_service1 =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service1->registry(), /*country=*/"",
                                true);

  std::unique_ptr<TestingPrefServiceSimple> pref_service2 =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service2->registry(), /*country=*/"",
                                true);

  pref_service1->SetInteger(data_.pref_name, static_cast<int>(modifier_key_));
  pref_service2->SetInteger(data_.pref_name, static_cast<int>(modifier_key_));

  ash_test_helper()->test_session_controller_client()->SetUserPrefService(
      account_id1, std::move(pref_service1));
  ash_test_helper()->test_session_controller_client()->SetUserPrefService(
      account_id2, std::move(pref_service2));

  ResetHistogramTester();

  // Sign into first account and verify the metric is emitted.
  SimulateUserLogin(account_id1);
  if (modifier_key_ != data_.default_modifier_key) {
    histogram_tester_->ExpectUniqueSample(data_.started_metric_name,
                                          static_cast<int>(modifier_key_), 1);
  } else {
    histogram_tester_->ExpectUniqueSample(data_.started_metric_name,
                                          static_cast<int>(modifier_key_), 0);
  }

  // Sign into second account and verify the metric is emitted.
  SimulateUserLogin(account_id2);
  if (modifier_key_ != data_.default_modifier_key) {
    histogram_tester_->ExpectUniqueSample(data_.started_metric_name,
                                          static_cast<int>(modifier_key_), 2);
  } else {
    histogram_tester_->ExpectUniqueSample(data_.started_metric_name,
                                          static_cast<int>(modifier_key_), 0);
  }

  // Sign back into the first account and verify no more metrics are emitted.
  SimulateUserLogin(account_id1);
  if (modifier_key_ != data_.default_modifier_key) {
    histogram_tester_->ExpectUniqueSample(data_.started_metric_name,
                                          static_cast<int>(modifier_key_), 2);
  } else {
    histogram_tester_->ExpectUniqueSample(data_.started_metric_name,
                                          static_cast<int>(modifier_key_), 0);
  }
}

// Contains a list of modifier remappings to apply and then the expected hash
// value after a user signs in. If `expected_value` is empty, then no metric is
// expected.
struct KeyboardModifierMetricsRecorderHashTestData {
  base::flat_map<std::string, ui::mojom::ModifierKey> modifier_remappings;
  std::optional<int32_t> expected_value;
};

class KeyboardModifierMetricsRecorderHashTest
    : public KeyboardModifierMetricsRecorderTest,
      public testing::WithParamInterface<
          KeyboardModifierMetricsRecorderHashTestData> {
  void SetUp() override {
    KeyboardModifierMetricsRecorderTest::SetUp();
    data_ = GetParam();
    ResetHistogramTester();
  }

 protected:
  KeyboardModifierMetricsRecorderHashTestData data_;
};

// Contains lists of modifier remappings to apply before signing in the user and
// then the expected computed hash once the user signs in.
INSTANTIATE_TEST_SUITE_P(
    ,
    KeyboardModifierMetricsRecorderHashTest,
    testing::ValuesIn(std::vector<KeyboardModifierMetricsRecorderHashTestData>{
        // With only default remappings, no metric is expected.
        {{}, std::nullopt},

        // All keys remapped to `ui::mojom::ModifierKey::kMeta` should hash to
        // 0.
        {{{::prefs::kLanguageRemapAltKeyTo, ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapCapsLockKeyTo, ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapBackspaceKeyTo,
           ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapEscapeKeyTo, ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapControlKeyTo, ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapAssistantKeyTo,
           ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapExternalMetaKeyTo,
           ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapExternalCommandKeyTo,
           ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapSearchKeyTo, ui::mojom::ModifierKey::kMeta}},
         0x0},

        // All keys remapped to `ui::mojom::ModifierKey::kBackspace` should hash
        // to
        // 0x6db6db6.
        {{{::prefs::kLanguageRemapAltKeyTo, ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapCapsLockKeyTo,
           ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapBackspaceKeyTo,
           ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapEscapeKeyTo,
           ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapControlKeyTo,
           ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapAssistantKeyTo,
           ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapExternalMetaKeyTo,
           ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapExternalCommandKeyTo,
           ui::mojom::ModifierKey::kBackspace},
          {::prefs::kLanguageRemapSearchKeyTo,
           ui::mojom::ModifierKey::kBackspace}},
         0x6db6db6},

        // Random assortment of keys with a manually computed hash.
        {{{::prefs::kLanguageRemapAltKeyTo, ui::mojom::ModifierKey::kControl},
          {::prefs::kLanguageRemapCapsLockKeyTo, ui::mojom::ModifierKey::kAlt},
          {::prefs::kLanguageRemapBackspaceKeyTo,
           ui::mojom::ModifierKey::kAssistant},
          {::prefs::kLanguageRemapEscapeKeyTo, ui::mojom::ModifierKey::kVoid},
          {::prefs::kLanguageRemapControlKeyTo, ui::mojom::ModifierKey::kMeta},
          {::prefs::kLanguageRemapAssistantKeyTo, ui::mojom::ModifierKey::kAlt},
          {::prefs::kLanguageRemapExternalMetaKeyTo,
           ui::mojom::ModifierKey::kControl},
          {::prefs::kLanguageRemapExternalCommandKeyTo,
           ui::mojom::ModifierKey::kCapsLock},
          {::prefs::kLanguageRemapSearchKeyTo, ui::mojom::ModifierKey::kAlt}},
         0x4452ec1},
    }));

TEST_P(KeyboardModifierMetricsRecorderHashTest, HashTest) {
  // Initialize two pref services with the initial value of the metric.
  const AccountId account_id1 = AccountId::FromUserEmail(kUserEmail1);
  const AccountId account_id2 = AccountId::FromUserEmail(kUserEmail2);

  std::unique_ptr<TestingPrefServiceSimple> pref_service1 =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service1->registry(), /*country=*/"",
                                true);

  std::unique_ptr<TestingPrefServiceSimple> pref_service2 =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service2->registry(), /*country=*/"",
                                true);

  for (const auto& [pref, remapping] : data_.modifier_remappings) {
    pref_service1->SetInteger(pref, static_cast<int>(remapping));
    pref_service2->SetInteger(pref, static_cast<int>(remapping));
  }

  ash_test_helper()->test_session_controller_client()->SetUserPrefService(
      account_id1, std::move(pref_service1));
  ash_test_helper()->test_session_controller_client()->SetUserPrefService(
      account_id2, std::move(pref_service2));

  ResetHistogramTester();

  // Sign into first account and verify the metric is emitted.
  SimulateUserLogin(account_id1);
  if (data_.expected_value.has_value()) {
    histogram_tester_->ExpectUniqueSample(
        "ChromeOS.Settings.Keyboard.Modifiers.Hash",
        data_.expected_value.value(), 1);
  } else {
    histogram_tester_->ExpectTotalCount(
        "ChromeOS.Settings.Keyboard.Modifiers.Hash", 0);
  }

  // Sign into second account and verify the metric is emitted.
  SimulateUserLogin(account_id2);
  if (data_.expected_value.has_value()) {
    histogram_tester_->ExpectUniqueSample(
        "ChromeOS.Settings.Keyboard.Modifiers.Hash",
        data_.expected_value.value(), 2);
  } else {
    histogram_tester_->ExpectTotalCount(
        "ChromeOS.Settings.Keyboard.Modifiers.Hash", 0);
  }

  ResetHistogramTester();

  // Sign back into first  account and verify the metric is not emitted again.
  SimulateUserLogin(account_id1);
  histogram_tester_->ExpectTotalCount(
      "ChromeOS.Settings.Keyboard.Modifiers.Hash", 0);
}

}  // namespace ash
