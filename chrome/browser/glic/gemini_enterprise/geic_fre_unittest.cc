// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

using prefs::GeicFreStatus;

constexpr char kGuestUrl[] =
    "https://business.gemini.google/side-panel?configId=abc&hl=en";

// `kGuestUrl` normalized to the `kGeicCompletedFre` key.
constexpr char kFreKey[] =
    "https://business.gemini.google/side-panel?configId=abc";

// Key of the status in each `kGeicCompletedFre` entry. Spelled out rather than
// shared with geic_fre.cc, since it is persisted and must not change.
constexpr char kStatusKey[] = "status";

// Enables GEiC and the GEiC FRE in `feature_list`, with `guest_url` as the
// `features::kGeicGuestURL` Finch param.
void InitGeicFeatures(base::test::ScopedFeatureList& feature_list,
                      const std::string& guest_url) {
  feature_list.InitWithFeaturesAndParameters(
      {{features::kGeic, {{"enabled", "true"}, {"geic-guest-url", guest_url}}},
       {features::kGeicFre, {}}},
      // Not needed for the GEiC FRE. Keeps profiles from being Glic-eligible,
      // which would create a `GlicKeyedService` this fixture doesn't support.
      {features::kGlic});
}

class GeicFreTest : public testing::Test {
 public:
  void SetUp() override {
    InitGeicFeatures(feature_list_, kGuestUrl);
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override { profile_.reset(); }

 protected:
  PrefService* prefs() { return profile_->GetPrefs(); }

  // Writes `entry` for `key` directly into the pref.
  void SetRawEntry(const std::string& key, base::Value entry) {
    ScopedDictPrefUpdate update(prefs(), prefs::kGeicCompletedFre);
    update->Set(key, std::move(entry));
  }

  void SetRawStatus(base::Value status) {
    SetRawEntry(kFreKey, base::Value(base::DictValue().Set(kStatusKey,
                                                           std::move(status))));
  }

  // Returns the `kGeicCompletedFre` key that a profile latched to `guest_url`
  // stores its GEiC FRE status under, or "" if it stores nothing.
  std::string GetFreKeyForGuestUrl(const std::string& guest_url) {
    base::test::ScopedFeatureList feature_list;
    InitGeicFeatures(feature_list, guest_url);
    TestingProfile profile;
    CompleteFre(&profile);
    const base::DictValue& dict =
        profile.GetPrefs()->GetDict(prefs::kGeicCompletedFre);
    if (dict.size() != 1u) {
      ADD_FAILURE() << "Expected one entry for " << guest_url << ", got "
                    << dict.size();
      return std::string();
    }
    return dict.begin()->first;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(GeicFreTest, EnabledWithFeatureAndGeic) {
  EXPECT_TRUE(GetFreStatus(profile_.get()).has_value());
  EXPECT_EQ(GetFreStatus(nullptr), std::nullopt);
}

TEST_F(GeicFreTest, DisabledWithoutGeicFreFeature) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(features::kGeicFre);
  TestingProfile profile;

  EXPECT_EQ(GetFreStatus(&profile), std::nullopt);

  CompleteFre(&profile);
  EXPECT_TRUE(profile.GetPrefs()->GetDict(prefs::kGeicCompletedFre).empty());
}

TEST_F(GeicFreTest, DisabledWithoutGeic) {
  base::test::ScopedFeatureList disabled;
  disabled.InitAndDisableFeature(features::kGeic);
  TestingProfile profile;

  EXPECT_EQ(GetFreStatus(&profile), std::nullopt);
  CompleteFre(&profile);
  EXPECT_TRUE(profile.GetPrefs()->GetDict(prefs::kGeicCompletedFre).empty());
}

TEST_F(GeicFreTest, DefaultsToNotStarted) {
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);
}

TEST_F(GeicFreTest, CompleteFreWritesStatus) {
  CompleteFre(profile_.get());

  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kCompleted);

  const base::DictValue& dict = prefs()->GetDict(prefs::kGeicCompletedFre);
  ASSERT_EQ(dict.size(), 1u);
  const base::DictValue* entry = dict.FindDict(kFreKey);
  ASSERT_TRUE(entry);
  EXPECT_EQ(entry->FindInt(kStatusKey),
            std::to_underlying(GeicFreStatus::kCompleted));
}

TEST_F(GeicFreTest, CompleteFreIsNoOpWhenAlreadyCompleted) {
  int pref_changes = 0;
  PrefChangeRegistrar registrar;
  registrar.Init(prefs());
  registrar.Add(prefs::kGeicCompletedFre,
                base::BindLambdaForTesting([&] { ++pref_changes; }));

  CompleteFre(profile_.get());
  EXPECT_EQ(pref_changes, 1);

  // Completing again doesn't touch the pref, so observers aren't notified.
  CompleteFre(profile_.get());
  EXPECT_EQ(pref_changes, 1);
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kCompleted);
}

TEST_F(GeicFreTest, ReadsFailClosedOnInvalidEntries) {
  // Missing status.
  SetRawEntry(kFreKey, base::Value(base::DictValue()));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);

  // Entry isn't a dict.
  SetRawEntry(kFreKey, base::Value(1));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);

  // Non-int status.
  SetRawStatus(base::Value("accepted"));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);
  SetRawStatus(base::Value(true));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);

  // Out-of-range values.
  SetRawStatus(base::Value(std::to_underlying(GeicFreStatus::kMaxValue) + 1));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);
  SetRawStatus(base::Value(std::to_underlying(GeicFreStatus::kMinValue) - 1));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);

  // Valid value.
  SetRawStatus(base::Value(std::to_underlying(GeicFreStatus::kCompleted)));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kCompleted);
}

TEST_F(GeicFreTest, OtherInstanceStatusDoesNotApply) {
  constexpr char kOtherUrl[] =
      "https://business.gemini.google/side-panel?configId=other";
  SetRawEntry(kOtherUrl,
              base::Value(base::DictValue().Set(
                  kStatusKey, std::to_underlying(GeicFreStatus::kCompleted))));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);

  // Completing this instance reads as completed and leaves the other entry.
  CompleteFre(profile_.get());
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kCompleted);
  const base::DictValue& dict = prefs()->GetDict(prefs::kGeicCompletedFre);
  EXPECT_EQ(dict.size(), 2u);
  EXPECT_TRUE(dict.FindDict(kOtherUrl));
}

TEST_F(GeicFreTest, LatchesGuestUrl) {
  // The guest URL is latched with the profile's Glic provider when the profile
  // is created, so a later policy URL doesn't change which GE instance the FRE
  // status is stored for.
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);
  prefs()->SetString(glic::prefs::kGlicGeminiEnterpriseUrl,
                     "https://business.gemini.google/home/cid/other");

  CompleteFre(profile_.get());
  const base::DictValue& dict = prefs()->GetDict(prefs::kGeicCompletedFre);
  EXPECT_EQ(dict.size(), 1u);
  EXPECT_TRUE(dict.FindDict(kFreKey));
}

TEST_F(GeicFreTest, CompletionSurvivesLocaleChange) {
  // Without `hl` in the guest URL, `GetLocalizedGuestURL()` appends the UI
  // locale, so the guest URL changes with Chrome's UI language.
  TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
  const std::string original_locale = browser_process->GetApplicationLocale();
  base::test::ScopedFeatureList feature_list;
  InitGeicFeatures(feature_list,
                   "https://business.gemini.google/side-panel?configId=abc");
  TestingProfile profile;

  browser_process->SetApplicationLocale("en-US");
  const GURL en_guest_url =
      glic::GetGlicProviderSpec(&profile).GetGuestUrl(&profile);
  CompleteFre(&profile);
  EXPECT_EQ(GetFreStatus(&profile), GeicFreStatus::kCompleted);

  browser_process->SetApplicationLocale("fr");
  EXPECT_NE(glic::GetGlicProviderSpec(&profile).GetGuestUrl(&profile),
            en_guest_url);
  EXPECT_EQ(GetFreStatus(&profile), GeicFreStatus::kCompleted);
  const base::DictValue& dict =
      profile.GetPrefs()->GetDict(prefs::kGeicCompletedFre);
  EXPECT_EQ(dict.size(), 1u);
  EXPECT_TRUE(dict.FindDict(kFreKey));

  browser_process->SetApplicationLocale(original_locale);
}

// `GetFrePrefKey()` only strips `hl` from `GetGuestUrl()`. If this fails
// because `GetGuestUrl()` adds another per-session param, strip it from the key
// too.
TEST_F(GeicFreTest, GuestUrlOnlyAddsLocaleParam) {
  constexpr char kConfiguredUrl[] =
      "https://business.gemini.google/side-panel?configId=abc";
  base::test::ScopedFeatureList feature_list;
  InitGeicFeatures(feature_list, kConfiguredUrl);
  TestingProfile profile;

  const GURL guest_url =
      glic::GetGlicProviderSpec(&profile).GetGuestUrl(&profile);
  EXPECT_TRUE(guest_url.spec().starts_with(kConfiguredUrl)) << guest_url;
  std::vector<std::string> keys;
  for (net::QueryIterator it(guest_url); !it.IsAtEnd(); it.Advance()) {
    keys.emplace_back(it.GetKey());
  }
  EXPECT_EQ(keys, (std::vector<std::string>{"configId", "hl"}));
}

TEST_F(GeicFreTest, KeyDropsOnlyLocaleParam) {
  EXPECT_EQ(GetFreKeyForGuestUrl("https://business.gemini.google/side-panel?"
                                 "foo=bar&hl=de&configId=abc#frag"),
            "https://business.gemini.google/side-panel?foo=bar&configId=abc"
            "#frag");
  EXPECT_EQ(GetFreKeyForGuestUrl(
                "https://business.gemini.google/side-panel?configId="),
            "https://business.gemini.google/side-panel?configId=");
  EXPECT_EQ(GetFreKeyForGuestUrl("https://business.gemini.google"),
            "https://business.gemini.google/");
}

TEST_F(GeicFreTest, HomeCidAndSidePanelUrlsShareKey) {
  EXPECT_EQ(GetFreKeyForGuestUrl("https://business.gemini.google/home/cid/abc"),
            kFreKey);
  EXPECT_EQ(
      GetFreKeyForGuestUrl("https://business.gemini.google/home/cid/abc?hl=de"),
      kFreKey);

  // An entry stored for the `/home/cid/` form applies to the side-panel form.
  SetRawEntry(
      GetFreKeyForGuestUrl("https://business.gemini.google/home/cid/abc"),
      base::Value(base::DictValue().Set(
          kStatusKey, std::to_underlying(GeicFreStatus::kCompleted))));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kCompleted);
}

TEST_F(GeicFreTest, DifferentConfigIdsAndOriginsGetSeparateKeys) {
  EXPECT_EQ(GetFreKeyForGuestUrl(
                "https://business.gemini.google/side-panel?configId=other"),
            "https://business.gemini.google/side-panel?configId=other");
  EXPECT_EQ(
      GetFreKeyForGuestUrl(
          "https://staging.business.gemini.google/side-panel?configId=abc"),
      "https://staging.business.gemini.google/side-panel?configId=abc");
  EXPECT_EQ(GetFreKeyForGuestUrl(
                "https://business.gemini.google:8443/side-panel?configId=abc"),
            "https://business.gemini.google:8443/side-panel?configId=abc");
}

TEST_F(GeicFreTest, IndependentOfConsumerFre) {
  prefs()->SetInteger(glic::prefs::kGlicCompletedFre,
                      std::to_underlying(glic::prefs::FreStatus::kCompleted));
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);

  prefs()->SetInteger(glic::prefs::kGlicCompletedFre,
                      std::to_underlying(glic::prefs::FreStatus::kNotStarted));
  CompleteFre(profile_.get());
  EXPECT_EQ(prefs()->GetInteger(glic::prefs::kGlicCompletedFre),
            std::to_underlying(glic::prefs::FreStatus::kNotStarted));
}

TEST_F(GeicFreTest, SkipFreSwitchReadsAsCompleted) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kGeicAlwaysSkipFre);
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kCompleted);
  // Nothing is persisted.
  EXPECT_TRUE(prefs()->GetDict(prefs::kGeicCompletedFre).empty());
}

TEST_F(GeicFreTest, ConsumerSkipFreSwitchDoesNotApply) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kGlicAlwaysSkipFre);
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);
}

TEST_F(GeicFreTest, AutomationSwitchDoesNotApply) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kGlicAutomation);
  EXPECT_EQ(GetFreStatus(profile_.get()), GeicFreStatus::kNotStarted);
}

}  // namespace
}  // namespace geic
