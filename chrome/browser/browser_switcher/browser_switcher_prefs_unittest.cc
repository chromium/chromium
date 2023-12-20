// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace browser_switcher {

namespace {

class TestBrowserSwitcherPrefs : public BrowserSwitcherPrefs {
 public:
  TestBrowserSwitcherPrefs(PrefService* prefs,
                           policy::PolicyService* policy_service)
      : BrowserSwitcherPrefs(prefs, policy_service) {}
};

base::Value StringArrayToValue(const std::vector<const char*>& strings) {
  base::Value::List list;
  for (const auto* string : strings)
    list.Append(string);
  return base::Value(std::move(list));
}

}  // namespace

class BrowserSwitcherPrefsTest : public testing::Test {
 public:
  void SetUp() override {
    BrowserSwitcherPrefs::RegisterProfilePrefs(prefs_backend_.registry());
    policy_provider_ = std::make_unique<
        testing::NiceMock<policy::MockConfigurationPolicyProvider>>();
    policy_provider_->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    std::vector<
        raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>
        providers = {policy_provider_.get()};
    policy_service_ = std::make_unique<policy::PolicyServiceImpl>(providers);
    prefs_ = std::make_unique<TestBrowserSwitcherPrefs>(&prefs_backend_,
                                                        policy_service_.get());
  }

  void TearDown() override { prefs_->Shutdown(); }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return policy_provider_.get();
  }
  sync_preferences::TestingPrefServiceSyncable* prefs_backend() {
    return &prefs_backend_;
  }
  BrowserSwitcherPrefs* prefs() { return prefs_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable prefs_backend_;

  std::unique_ptr<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<policy::PolicyService> policy_service_;
  std::unique_ptr<BrowserSwitcherPrefs> prefs_;
};

TEST_F(BrowserSwitcherPrefsTest, ListensForPrefChanges) {
  prefs_backend()->SetManagedPref(prefs::kEnabled, base::Value(true));
  prefs_backend()->SetManagedPref(prefs::kAlternativeBrowserPath,
                                  base::Value("notepad.exe"));
  prefs_backend()->SetManagedPref(prefs::kAlternativeBrowserParameters,
                                  StringArrayToValue({"a", "b", "c"}));
#if BUILDFLAG(IS_WIN)
  prefs_backend()->SetManagedPref(prefs::kChromePath, base::Value("cmd.exe"));
  prefs_backend()->SetManagedPref(prefs::kChromeParameters,
                                  StringArrayToValue({"d", "e", "f"}));
#endif
  prefs_backend()->SetManagedPref(prefs::kUrlList,
                                  StringArrayToValue({"example.com"}));
  prefs_backend()->SetManagedPref(prefs::kUrlGreylist,
                                  StringArrayToValue({"foo.example.com"}));

  EXPECT_EQ(true, prefs()->IsEnabled());

  EXPECT_EQ("notepad.exe", prefs()->GetAlternativeBrowserPath());

  EXPECT_EQ(3u, prefs()->GetAlternativeBrowserParameters().size());
  EXPECT_EQ("a", prefs()->GetAlternativeBrowserParameters()[0]);
  EXPECT_EQ("b", prefs()->GetAlternativeBrowserParameters()[1]);
  EXPECT_EQ("c", prefs()->GetAlternativeBrowserParameters()[2]);

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ("cmd.exe", prefs()->GetChromePath().MaybeAsASCII());

  EXPECT_EQ(3u, prefs()->GetChromeParameters().size());
  EXPECT_EQ("d", prefs()->GetChromeParameters()[0]);
  EXPECT_EQ("e", prefs()->GetChromeParameters()[1]);
  EXPECT_EQ("f", prefs()->GetChromeParameters()[2]);
#endif

  EXPECT_EQ(1u, prefs()->GetRules().sitelist.size());
  EXPECT_EQ("example.com", prefs()->GetRules().sitelist[0]->ToString());

  EXPECT_EQ(1u, prefs()->GetRules().greylist.size());
  EXPECT_EQ("foo.example.com", prefs()->GetRules().greylist[0]->ToString());
}

TEST_F(BrowserSwitcherPrefsTest, TriggersObserversOnPolicyChange) {
  policy::PolicyMap policy_map;
  policy_map.Set(policy::key::kAlternativeBrowserPath,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM, base::Value("notepad.exe"),
                 nullptr);

  base::RunLoop run_loop;
  auto subscription = prefs()->RegisterPrefsChangedCallback(base::BindRepeating(
      [](base::OnceClosure quit, BrowserSwitcherPrefs* prefs,
         const std::vector<std::string>& changed_prefs) {
        EXPECT_EQ("notepad.exe", prefs->GetAlternativeBrowserPath());
        std::vector<std::string> expected_changed_prefs{
            prefs::kAlternativeBrowserPath,
        };
        EXPECT_EQ(expected_changed_prefs, changed_prefs);
        std::move(quit).Run();
      },
      run_loop.QuitClosure()));

  prefs_backend()->SetManagedPref(prefs::kAlternativeBrowserPath,
                                  base::Value("notepad.exe"));
  policy_provider()->UpdateChromePolicy(policy_map);

  run_loop.Run();

  // If this code is reached, the callback has run as expected. Now just clean
  // up.
}

}  // namespace browser_switcher
