// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_switch_notice.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

constexpr char kGeicAUrl[] =
    "https://business.gemini.google/side-panel?configId=a";
constexpr char kGeicBUrl[] =
    "https://business.gemini.google/side-panel?configId=b";
// `prefs::kGlicProviderSwitchNoticeKnownProviderKey` for Gemini.
constexpr char kGeminiKey[] = "";

// Completes the GEiC FRE for the GE instance with `guest_url_key`.
void CompleteGeicFre(PrefService& prefs, const std::string& guest_url_key) {
  ScopedDictPrefUpdate(&prefs, geic::prefs::kGeicCompletedFre)
      ->Set(guest_url_key,
            base::DictValue().Set(
                "status",
                std::to_underlying(geic::prefs::GeicFreStatus::kCompleted)));
}

class GlicProviderSwitchNoticeTest : public testing::Test {
 protected:
  // New profiles latch to GEiC at `finch_guest_url`, or to Gemini if it's
  // empty or `geic_enabled` is false.
  void InitFeatures(const std::string& finch_guest_url,
                    bool notice_enabled = true,
                    bool geic_enabled = true) {
    base::FieldTrialParams geic_params;
    if (!finch_guest_url.empty()) {
      geic_params["geic-guest-url"] = finch_guest_url;
    }
    std::vector<base::test::FeatureRefAndParams> enabled;
    std::vector<base::test::FeatureRef> disabled;
    if (geic_enabled) {
      enabled.emplace_back(features::kGeic, geic_params);
    } else {
      disabled.push_back(features::kGeic);
    }
    if (notice_enabled) {
      enabled.push_back({features::kGeicProviderSwitchNotice, {}});
    } else {
      disabled.push_back(features::kGeicProviderSwitchNotice);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled, disabled);
  }

  // Returns prefs from a previous session: `known_key` (unset if std::nullopt)
  // and the FRE completed for each of `onboarded_keys` ("" for Gemini).
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> CreatePrefs(
      std::optional<std::string> known_key,
      const std::vector<std::string>& onboarded_keys = {}) {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    if (known_key) {
      prefs->SetString(prefs::kGlicProviderSwitchNoticeKnownProviderKey,
                       *known_key);
    }
    for (const std::string& key : onboarded_keys) {
      if (key.empty()) {
        prefs->SetInteger(prefs::kGlicCompletedFre,
                          std::to_underlying(prefs::FreStatus::kCompleted));
      } else {
        CompleteGeicFre(*prefs, key);
      }
    }
    return prefs;
  }

  // Creates the profile, which latches its provider.
  TestingProfile* StartUp(
      std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs) {
    TestingProfile::Builder builder;
    // The tests drive the switch notice directly. Without this, a profile that
    // is Glic-eligible (e.g. already onboarded to Gemini on Android) would
    // build `GlicKeyedService`, which needs a `ProfileManager`.
    builder.AddTestingFactory(
        GlicKeyedServiceFactory::GetInstance(),
        BrowserContextKeyedServiceFactory::TestingFactory());
    if (prefs) {
      builder.SetPrefService(std::move(prefs));
    }
    profile_ = builder.Build();
    return profile_.get();
  }

  // Returns the known provider, or std::nullopt if none is known.
  std::optional<GlicProviderKey> KnownProviderKey() {
    const PrefService& prefs = *profile_->GetPrefs();
    if (!prefs.HasPrefPath(prefs::kGlicProviderSwitchNoticeKnownProviderKey)) {
      return std::nullopt;
    }
    return GlicProviderKey::FromPrefString(
        prefs.GetString(prefs::kGlicProviderSwitchNoticeKnownProviderKey));
  }

  // Returns the latched provider's key.
  GlicProviderKey LatchedProviderKey() {
    return GetGlicProviderSpec(profile_.get()).GetProviderKeyForTesting();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(GlicProviderSwitchNoticeTest, FirstStartupNotOnboardedStaysUnset) {
  InitFeatures(kGeicAUrl);
  TestingProfile* profile = StartUp(nullptr);
  ASSERT_EQ(LatchedProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));

  EXPECT_EQ(KnownProviderKey(), std::nullopt);
  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
}

// E.g. a user who onboarded before this was tracked.
TEST_F(GlicProviderSwitchNoticeTest, FirstStartupOnboardedRecordsLatched) {
  InitFeatures(kGeicAUrl);
  TestingProfile* profile = StartUp(
      CreatePrefs(/*known_key=*/std::nullopt, /*onboarded_keys=*/{kGeicAUrl}));

  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
}

TEST_F(GlicProviderSwitchNoticeTest, GeminiToGeic) {
  InitFeatures(kGeicAUrl);
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey}));
  ASSERT_EQ(LatchedProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));

  EXPECT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeminiToGeic);
  // Kept, so the notice stays due.
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
}

TEST_F(GlicProviderSwitchNoticeTest, GeicToGemini) {
  InitFeatures(/*finch_guest_url=*/"");
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeicAUrl}));
  ASSERT_EQ(LatchedProviderKey(), GlicProviderKey::Gemini());

  EXPECT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeicToGemini);
}

TEST_F(GlicProviderSwitchNoticeTest, GeicToOtherGeic) {
  InitFeatures(kGeicBUrl);
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeicAUrl}));

  EXPECT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeicToOtherGeic);
}

// The new provider's FRE doesn't matter: the notice is shown before it.
TEST_F(GlicProviderSwitchNoticeTest, NewProviderAlreadyOnboarded) {
  InitFeatures(kGeicBUrl);
  TestingProfile* profile = StartUp(
      CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeicAUrl, kGeicBUrl}));

  EXPECT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeicToOtherGeic);
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
}

TEST_F(GlicProviderSwitchNoticeTest, SameProviderIsNone) {
  InitFeatures(kGeicAUrl);
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeicAUrl}));

  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
}

// A user-initiated switch clears a notice that's due.
TEST_F(GlicProviderSwitchNoticeTest, UserInitiatedSwitchClearsNotice) {
  InitFeatures(kGeicAUrl);
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey}));
  ASSERT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeminiToGeic);

  RecordUserInitiatedProviderSwitch(*profile);

  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
  EXPECT_EQ(KnownProviderKey(), std::nullopt);
}

// The switch the user makes themselves isn't reported at the next startup.
TEST_F(GlicProviderSwitchNoticeTest, UserInitiatedSwitchNotReported) {
  // The user switches away from Gemini themselves.
  InitFeatures(/*finch_guest_url=*/"");
  StartUp(CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey, kGeicAUrl}));
  RecordUserInitiatedProviderSwitch(*profile_);
  ASSERT_EQ(KnownProviderKey(), std::nullopt);
  profile_.reset();

  // Next startup: latched to GEiC A.
  feature_list_.Reset();
  InitFeatures(kGeicAUrl);
  TestingProfile* profile = StartUp(CreatePrefs(
      /*known_key=*/std::nullopt, /*onboarded_keys=*/{kGeminiKey, kGeicAUrl}));

  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
}

// While notices are off, the known provider follows the latched one.
TEST_F(GlicProviderSwitchNoticeTest, FeatureDisabledRecordsLatched) {
  InitFeatures(kGeicAUrl, /*notice_enabled=*/false);
  TestingProfile* profile = StartUp(
      CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey, kGeicAUrl}));

  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
}

TEST_F(GlicProviderSwitchNoticeTest, FeatureDisabledNotOnboardedClears) {
  InitFeatures(kGeicAUrl, /*notice_enabled=*/false);
  StartUp(CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey}));

  EXPECT_EQ(KnownProviderKey(), std::nullopt);
}

// A switch while notices were off isn't reported once they're enabled.
TEST_F(GlicProviderSwitchNoticeTest, FeatureEnabledLaterIgnoresPastSwitch) {
  // The switch to GEiC A takes effect while notices are off.
  InitFeatures(kGeicAUrl, /*notice_enabled=*/false);
  StartUp(CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey, kGeicAUrl}));
  ASSERT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
  profile_.reset();

  // Next startup: notices on, still latched to GEiC A.
  feature_list_.Reset();
  InitFeatures(kGeicAUrl);
  TestingProfile* profile = StartUp(
      CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeminiKey, kGeicAUrl}));

  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
}

// Onboarding during a session records the provider, so a switch before the next
// startup is reported.
TEST_F(GlicProviderSwitchNoticeTest, OnboardingRecordsLatched) {
  InitFeatures(/*finch_guest_url=*/"");
  TestingProfile* profile = StartUp(CreatePrefs(/*known_key=*/std::nullopt));
  ASSERT_EQ(KnownProviderKey(), std::nullopt);

  profile->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kCompleted));
  OnLatchedProviderFreStatusChanged(*profile);

  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
}

// GEiC is recorded by its guest URL key.
TEST_F(GlicProviderSwitchNoticeTest, GeicOnboardingRecordsLatched) {
  InitFeatures(kGeicAUrl);
  TestingProfile* profile = StartUp(CreatePrefs(/*known_key=*/std::nullopt));
  ASSERT_EQ(KnownProviderKey(), std::nullopt);

  CompleteGeicFre(*profile->GetPrefs(), kGeicAUrl);
  OnLatchedProviderFreStatusChanged(*profile);

  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicAUrl));
}

TEST_F(GlicProviderSwitchNoticeTest, FreStatusChangeWithoutOnboardingIgnored) {
  InitFeatures(/*finch_guest_url=*/"");
  TestingProfile* profile = StartUp(CreatePrefs(/*known_key=*/std::nullopt));

  OnLatchedProviderFreStatusChanged(*profile);

  EXPECT_EQ(KnownProviderKey(), std::nullopt);
}

// While `kGeic` is off, no switch is reported, even the GEiC -> Gemini switch
// that turning it off caused.
TEST_F(GlicProviderSwitchNoticeTest, GeicDisabledRecordsLatched) {
  InitFeatures(kGeicAUrl, /*notice_enabled=*/true, /*geic_enabled=*/false);
  TestingProfile* profile = StartUp(
      CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeminiKey, kGeicAUrl}));
  ASSERT_EQ(LatchedProviderKey(), GlicProviderKey::Gemini());

  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
}

// Onboarding to the new provider before the notice is acknowledged doesn't
// clear it.
TEST_F(GlicProviderSwitchNoticeTest, NewProviderOnboardingKeepsNotice) {
  InitFeatures(kGeicAUrl);
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey}));
  ASSERT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeminiToGeic);

  CompleteGeicFre(*profile->GetPrefs(), kGeicAUrl);
  OnLatchedProviderFreStatusChanged(*profile);

  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
  EXPECT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeminiToGeic);
}

TEST_F(GlicProviderSwitchNoticeTest, SwitchAssistantSelection) {
  InitFeatures(/*finch_guest_url=*/"");
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeminiKey, /*onboarded_keys=*/{kGeminiKey}));
  ASSERT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());

  SwitchAssistantSelection(*profile, prefs::AssistantSelection::kEnterprise);

  EXPECT_EQ(profile->GetPrefs()->GetInteger(prefs::kGlicAssistantSelection),
            static_cast<int>(prefs::AssistantSelection::kEnterprise));
  // Marked as user-initiated.
  EXPECT_EQ(KnownProviderKey(), std::nullopt);
}

TEST_F(GlicProviderSwitchNoticeTest, AcknowledgeRecordsLatched) {
  InitFeatures(kGeicBUrl);
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeicAUrl}));
  ASSERT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeicToOtherGeic);

  AcknowledgeProviderSwitchNotice(*profile);

  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::GeminiEnterprise(kGeicBUrl));
  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
}

TEST_F(GlicProviderSwitchNoticeTest, AcknowledgeRecordsGemini) {
  InitFeatures(/*finch_guest_url=*/"");
  TestingProfile* profile =
      StartUp(CreatePrefs(kGeicAUrl, /*onboarded_keys=*/{kGeicAUrl}));
  ASSERT_EQ(GetProviderSwitchNotice(*profile),
            GlicProviderChange::kGeicToGemini);

  AcknowledgeProviderSwitchNotice(*profile);

  EXPECT_EQ(KnownProviderKey(), GlicProviderKey::Gemini());
  EXPECT_EQ(GetProviderSwitchNotice(*profile), GlicProviderChange::kNone);
}

}  // namespace
}  // namespace glic
