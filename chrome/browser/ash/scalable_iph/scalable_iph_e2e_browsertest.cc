// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/variations/field_trial_config/fieldtrial_testing_config.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr char kStudyName[] = "ScalableIphStudy";
constexpr size_t kParamsSizeLimit = 40 << 10;  // 40 KiByte

bool IsGoogleChrome() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

const variations::FieldTrialTestingStudy* FindStudy() {
  const variations::FieldTrialTestingConfig& config =
      variations::kFieldTrialConfig;
  for (const variations::FieldTrialTestingStudy& study : config.studies) {
    if (std::string(study.name) == kStudyName) {
      return &study;
    }
  }
  return nullptr;
}

const variations::FieldTrialTestingExperiment* FindExperiment(
    const variations::FieldTrialTestingStudy* study,
    const std::string& experiment_name) {
  for (const variations::FieldTrialTestingExperiment& experiment :
       study->experiments) {
    if (experiment.name == experiment_name) {
      return &experiment;
    }
  }
  return nullptr;
}

const base::Feature* GetFeature(const std::string_view& feature_name) {
  static const base::NoDestructor<std::vector<const base::Feature*>>
      supported_features({&ash::features::kScalableIph,
                          &ash::features::kHelpAppWelcomeTips,
                          &ash::features::kShelfLauncherNudge});
  for (const base::Feature* feature : *supported_features) {
    if (feature->name == feature_name) {
      return feature;
    }
  }

  feature_engagement::FeatureVector features =
      feature_engagement::GetAllFeatures();
  for (const base::Feature* feature : features) {
    if (feature->name == feature_name) {
      return feature;
    }
  }

  CHECK(false) << feature_name << " was not found.";

  return nullptr;
}

base::FieldTrialParams GetFieldTrialParams(
    const std::string_view& prefix,
    const variations::FieldTrialTestingExperiment* experiment,
    size_t& params_size) {
  base::FieldTrialParams params;

  for (const variations::FieldTrialTestingExperimentParams& experiment_params :
       experiment->params) {
    std::string param_key(experiment_params.key);
    if (!base::StartsWith(param_key, prefix)) {
      continue;
    }

    std::string param_value(experiment_params.value);
    params[param_key] = param_value;

    params_size += param_key.size();
    params_size += param_value.size();
  }

  return params;
}

void ApplyExperiment(
    feature_engagement::test::ScopedIphFeatureList* scoped_feature_list,
    const variations::FieldTrialTestingExperiment* experiment) {
  size_t params_size = 0;
  std::vector<base::test::FeatureRefAndParams> enable_features;
  for (const auto* enabled_feature : experiment->enable_features) {
    std::string_view feature_name(enabled_feature);

    // `ScopedIphFeatureList` uses `ScopedFeatureList` internally.
    // `ScopedFeatureList` creates a field trial for each feature. Parse params
    // and collect related params to a feature. This is test specific behavior.
    // IIUC single field trial is created for all features in a single study in
    // production code.
    enable_features.push_back(base::test::FeatureRefAndParams(
        *GetFeature(feature_name),
        GetFieldTrialParams(feature_name, experiment, params_size)));
  }

  // Ignore non-param part (e.g. Flags) size as it should be small enough.
  CHECK(params_size < kParamsSizeLimit)
      << "Config contains params of size with " << params_size
      << ". Our internal config size limit of ScalableIph component is "
      << kParamsSizeLimit
      << ". Field trial data has 256KiByte size limit. We share the limit with "
         "other field trials. See kFieldTrialAllocationSize in field_trial.cc "
         "about details.";

  std::vector<base::test::FeatureRef> disable_features;
  for (const auto* disabled_feature : experiment->disable_features) {
    disable_features.push_back(
        base::test::FeatureRef(*GetFeature(std::string(disabled_feature))));
  }

  // Enable `ScalableIphDebug` for easier debug.
  enable_features.push_back(
      base::test::FeatureRefAndParams(ash::features::kScalableIphDebug, {}));

  scoped_feature_list->InitAndEnableFeaturesWithParameters(enable_features,
                                                           disable_features);
}

class ScalableIphE2EBrowserTest : public ash::ScalableIphBrowserTestBase {
 public:
  explicit ScalableIphE2EBrowserTest(const std::string& experiment_name)
      : experiment_name_(experiment_name) {
    enable_mock_tracker_ = false;
  }

  void InitializeScopedFeatureList() override {
    const variations::FieldTrialTestingStudy* study = FindStudy();
    CHECK(study) << kStudyName
                 << " was not found in fieldtrial testing config.";

    const variations::FieldTrialTestingExperiment* experiment =
        FindExperiment(study, experiment_name_);
    CHECK(experiment) << experiment_name_ << "was not found in the study.";

    ApplyExperiment(&scoped_iph_feature_list_, experiment);
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    ScalableIphBrowserTestBase::SetUpDefaultCommandLine(command_line);

    command_line->RemoveSwitch(switches::kDisableDefaultApps);
    command_line->AppendSwitch(
        ash::switches::kAllowDefaultShelfPinLayoutIgnoringSync);
  }

  void SetUpOnMainThread() override {
    ash::ScalableIphBrowserTestBase::SetUpOnMainThread();

    mock_delegate()->FakeShowBubble();
    mock_delegate()->FakeShowNotification();

    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();

    // Note that there is an async operation before network status change has
    // been propagated to `ScalableIph`. This does NOT wait it.
    AddOnlineNetwork();
  }

 private:
  const std::string experiment_name_;

  // TODO(b/285225729): consider if we can use `ScopedIphFeatureList` in
  // `ScalableIphBrowserTestBase`.
  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;
};

class ScalableIphE2EBrowserTestCounterfactualControl
    : public ScalableIphE2EBrowserTest {
 public:
  ScalableIphE2EBrowserTestCounterfactualControl()
      : ScalableIphE2EBrowserTest("CounterfactualControl_BETA_20231204") {}
};

class ScalableIphE2EBrowserTestUnlockedBased
    : public ScalableIphE2EBrowserTest {
 public:
  ScalableIphE2EBrowserTestUnlockedBased()
      : ScalableIphE2EBrowserTest("UnlockedBased_BETA_20231204") {}
};

class ScalableIphE2EBrowserTestTimerBased : public ScalableIphE2EBrowserTest {
 public:
  ScalableIphE2EBrowserTestTimerBased()
      : ScalableIphE2EBrowserTest("TimerBased_BETA_20231204") {}
};

class ScalableIphE2EBrowserTestHelpAppBased : public ScalableIphE2EBrowserTest {
 public:
  ScalableIphE2EBrowserTestHelpAppBased()
      : ScalableIphE2EBrowserTest("HelpAppBased_BETA_20231204") {}
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ScalableIphE2EBrowserTestCounterfactualControl, E2E) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP() << "E2E tests are designed to be run under Google Chrome";
  }

  EXPECT_FALSE(ash::ShelfModel::Get()->IsAppPinned(web_app::kHelpAppId));

  // TODO(b/285225729): add more expectations to test the config.
}

IN_PROC_BROWSER_TEST_F(ScalableIphE2EBrowserTestUnlockedBased, E2E) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP() << "E2E tests are designed to be run under Google Chrome";
  }

  EXPECT_FALSE(ash::ShelfModel::Get()->IsAppPinned(web_app::kHelpAppId));

  // TODO(b/285225729): add more expectations to test the config.
}

IN_PROC_BROWSER_TEST_F(ScalableIphE2EBrowserTestTimerBased, E2E) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP() << "E2E tests are designed to be run under Google Chrome";
  }

  EXPECT_FALSE(ash::ShelfModel::Get()->IsAppPinned(web_app::kHelpAppId));

  // TODO(b/285225729): add more expectations to test the config.
}

IN_PROC_BROWSER_TEST_F(ScalableIphE2EBrowserTestHelpAppBased, E2E) {
  if (!IsGoogleChrome()) {
    GTEST_SKIP() << "E2E tests are designed to be run under Google Chrome";
  }

  EXPECT_TRUE(ash::ShelfModel::Get()->IsAppPinned(web_app::kHelpAppId));

  // TODO(b/285225729): add more expectations to test the config.
}

