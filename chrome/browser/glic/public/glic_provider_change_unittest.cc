// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_change.h"

#include <initializer_list>
#include <string>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

// The policy holds the web app URL, which resolves to the side-panel URL.
constexpr char kGeicAUrl[] =
    "https://business.gemini.google/side-panel?configId=a";
constexpr char kGeicAPolicyUrl[] = "https://business.gemini.google/home/cid/a";
constexpr char kGeicBPolicyUrl[] = "https://business.gemini.google/home/cid/b";
// Not an allowed Gemini Enterprise host, so it resolves to no GEiC URL.
constexpr char kInvalidPolicyUrl[] = "https://example.com/";

// Spelled out rather than using `geic::kGeicGuestURLSwitch`: this target
// can't include geic_enabling.h (see chrome/browser/glic/DEPS).
constexpr char kGeicGuestUrlSwitch[] = "geic-guest-url";

// Real latches, with the configuration changed after the profile is created.
class GlicProviderChangeTest : public testing::Test {
 protected:
  // Profiles latch to `finch_guest_url` if it's not empty.
  void InitFeatures(const std::string& finch_guest_url = "") {
    base::FieldTrialParams geic_params;
    if (!finch_guest_url.empty()) {
      geic_params["geic-guest-url"] = finch_guest_url;
    }
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic, geic_params},
         {features::kGeicProviderChangePrompt, {}}},
        {});
  }

  void SetPolicyUrl(const std::string& url) {
    profile()->GetPrefs()->SetString(prefs::kGlicGeminiEnterpriseUrl, url);
  }

  void ClearPolicyUrl() {
    profile()->GetPrefs()->ClearPref(prefs::kGlicGeminiEnterpriseUrl);
  }

  void SetAssistantSelection(prefs::AssistantSelection selection) {
    profile()->GetPrefs()->SetInteger(prefs::kGlicAssistantSelection,
                                      std::to_underlying(selection));
  }

  Profile* profile() { return helper_.regular_profile(); }

  GlicProviderChange GetPendingProviderChange() {
    return GlicEnabling::GetPendingProviderChange(profile());
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  ProfileTestingHelper helper_;
};

class GlicProviderChangeGeminiLatchedTest : public GlicProviderChangeTest {
 public:
  void SetUp() override {
    InitFeatures();
    helper_.SetUp();
    ASSERT_EQ(GlicProviderLatch::GetProvider(*profile()),
              GlicProvider::kGemini);
  }
};

TEST_F(GlicProviderChangeGeminiLatchedTest, NoChange) {
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

TEST_F(GlicProviderChangeGeminiLatchedTest, PolicyAddedIsGeminiToGeic) {
  SetPolicyUrl(kGeicAPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeminiToGeic);
}

TEST_F(GlicProviderChangeGeminiLatchedTest, PolicyAddedThenRemovedIsNone) {
  SetPolicyUrl(kGeicAPolicyUrl);
  ASSERT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeminiToGeic);

  ClearPolicyUrl();
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

TEST_F(GlicProviderChangeGeminiLatchedTest, InvalidPolicyIsNone) {
  SetPolicyUrl(kInvalidPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

// A profile that selected consumer Gemini stays on Gemini after a relaunch, so
// a GEiC policy isn't a pending change.
TEST_F(GlicProviderChangeGeminiLatchedTest,
       PolicyAddedWithConsumerSelectedIsNone) {
  SetAssistantSelection(prefs::AssistantSelection::kConsumer);
  SetPolicyUrl(kGeicAPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

TEST_F(GlicProviderChangeGeminiLatchedTest,
       PolicyAddedWithEnterpriseSelectedIsGeminiToGeic) {
  SetAssistantSelection(prefs::AssistantSelection::kEnterprise);
  SetPolicyUrl(kGeicAPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeminiToGeic);
}

// Selecting consumer Gemini later reports the change, until it's reverted.
TEST_F(GlicProviderChangeGeminiLatchedTest,
       ConsumerSelectionTogglesPendingGeminiToGeic) {
  SetPolicyUrl(kGeicAPolicyUrl);
  ASSERT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeminiToGeic);

  SetAssistantSelection(prefs::AssistantSelection::kConsumer);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);

  SetAssistantSelection(prefs::AssistantSelection::kAuto);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeminiToGeic);
}

TEST_F(GlicProviderChangeGeminiLatchedTest, LatchIsUnchanged) {
  SetPolicyUrl(kGeicAPolicyUrl);
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile()), GlicProvider::kGemini);
}

TEST_F(GlicProviderChangeGeminiLatchedTest, UnlatchedProfilesAreNone) {
  SetPolicyUrl(kGeicAPolicyUrl);
  ASSERT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeminiToGeic);

  for (Profile* unlatched : std::initializer_list<Profile*>{
           helper_.incognito_profile(), helper_.isolated_mode_profile(),
           helper_.guest_profile(), helper_.guest_profile_otr()}) {
    EXPECT_FALSE(GlicProviderLatch::GetIfLatched(*unlatched));
    EXPECT_EQ(GlicEnabling::GetPendingProviderChange(unlatched),
              GlicProviderChange::kNone);
  }
}

TEST_F(GlicProviderChangeGeminiLatchedTest, NullContextIsNone) {
  EXPECT_EQ(GlicEnabling::GetPendingProviderChange(nullptr),
            GlicProviderChange::kNone);
}

// Latched to GEiC A through Finch. The policy takes precedence once set.
class GlicProviderChangeGeicLatchedTest : public GlicProviderChangeTest {
 public:
  void SetUp() override {
    InitFeatures(kGeicAUrl);
    helper_.SetUp();
    ASSERT_EQ(GlicProviderLatch::GetProvider(*profile()),
              GlicProvider::kGeminiEnterprise);
  }
};

TEST_F(GlicProviderChangeGeicLatchedTest, NoChange) {
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

TEST_F(GlicProviderChangeGeicLatchedTest, PolicyForOtherGeicIsGeicToOtherGeic) {
  SetPolicyUrl(kGeicBPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeicToOtherGeic);
}

// Only the source changed (Finch -> policy), not the guest URL.
TEST_F(GlicProviderChangeGeicLatchedTest, PolicyForSameGeicIsNone) {
  SetPolicyUrl(kGeicAPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

TEST_F(GlicProviderChangeGeicLatchedTest, PolicyModifiedThenRevertedIsNone) {
  SetPolicyUrl(kGeicBPolicyUrl);
  ASSERT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeicToOtherGeic);

  ClearPolicyUrl();
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

// An invalid policy URL still takes precedence over Finch.
TEST_F(GlicProviderChangeGeicLatchedTest, InvalidPolicyIsGeicToGemini) {
  SetPolicyUrl(kInvalidPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeicToGemini);
}

// Selecting consumer Gemini reports the change, until it's reverted.
TEST_F(GlicProviderChangeGeicLatchedTest,
       ConsumerSelectionTogglesPendingGeicToGemini) {
  SetAssistantSelection(prefs::AssistantSelection::kConsumer);
  ASSERT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeicToGemini);

  SetAssistantSelection(prefs::AssistantSelection::kAuto);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

// `--geic-guest-url` takes precedence over Finch. GEiC guest URLs are compared
// without `hl`.
TEST_F(GlicProviderChangeGeicLatchedTest, GuestUrlDiffersOnlyInLocaleIsNone) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      kGeicGuestUrlSwitch, std::string(kGeicAUrl) + "&hl=fr");
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

// Any guest URL difference other than `hl` is a different GEiC.
TEST_F(GlicProviderChangeGeicLatchedTest,
       GuestUrlWithOtherQueryParamIsGeicToOtherGeic) {
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      kGeicGuestUrlSwitch, std::string(kGeicAUrl) + "&location=eu");
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeicToOtherGeic);
}

// Latched to GEiC A through `--geic-guest-url`, which is then removed. This
// stands in for a removed policy: it's the only other source that can be
// removed after latching.
class GlicProviderChangeGeicRemovedTest : public GlicProviderChangeTest {
 public:
  void SetUp() override {
    InitFeatures();
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        kGeicGuestUrlSwitch, kGeicAUrl);
    helper_.SetUp();
    ASSERT_EQ(GlicProviderLatch::GetProvider(*profile()),
              GlicProvider::kGeminiEnterprise);
  }
};

TEST_F(GlicProviderChangeGeicRemovedTest, ConfigurationRemovedIsGeicToGemini) {
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
      kGeicGuestUrlSwitch);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kGeicToGemini);
}

class GlicProviderChangeFeatureDisabledTest : public GlicProviderChangeTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures({features::kGeic},
                                   {features::kGeicProviderChangePrompt});
    helper_.SetUp();
  }
};

// `GlicEnabling` reports no change while `kGeicProviderChangePrompt` is
// disabled, even though one is pending.
TEST_F(GlicProviderChangeFeatureDisabledTest, ReportsNone) {
  SetPolicyUrl(kGeicAPolicyUrl);
  EXPECT_EQ(GlicEnabling::GetPendingProviderChange(profile()),
            GlicProviderChange::kNone);
}

// `kGeic` enabled, but with its `enabled` param set to false.
class GlicProviderChangeGeicEnabledParamFalseTest
    : public GlicProviderChangeTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGeic, {{"enabled", "false"}}},
         {features::kGeicProviderChangePrompt, {}}},
        {});
    helper_.SetUp();
    ASSERT_EQ(GlicProviderLatch::GetProvider(*profile()),
              GlicProvider::kGemini);
  }
};

// With GEiC turned off, the configuration resolves to Gemini, so a policy URL
// isn't a pending change.
TEST_F(GlicProviderChangeGeicEnabledParamFalseTest, PolicyAddedIsNone) {
  SetPolicyUrl(kGeicAPolicyUrl);
  EXPECT_EQ(GetPendingProviderChange(), GlicProviderChange::kNone);
}

}  // namespace
}  // namespace glic
