// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_switch_notice.h"

#include "base/feature_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_fre_store.h"
#include "chrome/browser/glic/public/glic_provider_change.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace glic {

namespace {

// `features::kGeic` stays the overall kill switch: while it's off, no switch is
// reported, even one that turning it off caused.
bool IsProviderSwitchNoticeEnabled() {
  return geic::IsGeicEnabledByFeature() &&
         base::FeatureList::IsEnabled(features::kGeicProviderSwitchNotice);
}

bool OnboardedToLatchedProvider(Profile& profile,
                                const GlicProviderLatch& latch) {
  return latch.GetSpec().GetFreStore().GetStatus(&profile) ==
         prefs::FreStatus::kCompleted;
}

// Records the latched provider as known if no provider is known yet and the
// user has onboarded to it.
void MaybeInitKnownProvider(GlicProviderKeyPassKey key_pass_key,
                            Profile& profile,
                            const GlicProviderLatch& latch) {
  PrefService& prefs = *profile.GetPrefs();
  if (prefs.HasPrefPath(prefs::kGlicProviderSwitchNoticeKnownProviderKey)) {
    return;
  }
  if (!OnboardedToLatchedProvider(profile, latch)) {
    return;
  }
  prefs.SetString(prefs::kGlicProviderSwitchNoticeKnownProviderKey,
                  latch.GetSpec().GetProviderKey(key_pass_key).ToPrefString());
}

}  // namespace

void UpdateKnownProviderAtStartup(base::PassKey<GlicProviderLatchFactory>,
                                  Profile& profile) {
  const GlicProviderLatch* latch = GlicProviderLatch::GetIfLatched(profile);
  if (!latch) {
    return;
  }
  PrefService& prefs = *profile.GetPrefs();
  // While notices are off, there's no switch to tell the user about: forget the
  // old provider, so that only the latched one can become known.
  if (!IsProviderSwitchNoticeEnabled()) {
    prefs.ClearPref(prefs::kGlicProviderSwitchNoticeKnownProviderKey);
  }

  // Otherwise keep the known provider, so that a notice the user hasn't seen
  // yet stays due across restarts. If no provider is known, record the latched
  // one once the user has onboarded to it.
  MaybeInitKnownProvider(GlicProviderKeyPasskeyProvider::GetPassKey(), profile,
                         *latch);
}

void OnLatchedProviderFreStatusChanged(Profile& profile) {
  if (const GlicProviderLatch* latch =
          GlicProviderLatch::GetIfLatched(profile)) {
    MaybeInitKnownProvider(GlicProviderKeyPasskeyProvider::GetPassKey(),
                           profile, *latch);
  }
}

void RecordUserInitiatedProviderSwitch(Profile& profile) {
  profile.GetPrefs()->ClearPref(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey);
}

void SwitchAssistantSelection(Profile& profile,
                              prefs::AssistantSelection selection) {
  profile.GetPrefs()->SetInteger(prefs::kGlicAssistantSelection,
                                 static_cast<int>(selection));
  RecordUserInitiatedProviderSwitch(profile);
}

GlicProviderChange GetProviderSwitchNotice(Profile& profile) {
  if (!IsProviderSwitchNoticeEnabled()) {
    return GlicProviderChange::kNone;
  }
  const PrefService& prefs = *profile.GetPrefs();
  const GlicProviderLatch* latch = GlicProviderLatch::GetIfLatched(profile);
  if (!latch ||
      !prefs.HasPrefPath(prefs::kGlicProviderSwitchNoticeKnownProviderKey)) {
    return GlicProviderChange::kNone;
  }
  return ClassifyProviderChange(
      GlicProviderKey::FromPrefString(
          prefs.GetString(prefs::kGlicProviderSwitchNoticeKnownProviderKey)),
      latch->GetSpec().GetProviderKey(
          GlicProviderKeyPasskeyProvider::GetPassKey()));
}

void AcknowledgeProviderSwitchNotice(Profile& profile) {
  const GlicProviderLatch* latch = GlicProviderLatch::GetIfLatched(profile);
  if (!latch) {
    return;
  }
  profile.GetPrefs()->SetString(
      prefs::kGlicProviderSwitchNoticeKnownProviderKey,
      latch->GetSpec()
          .GetProviderKey(GlicProviderKeyPasskeyProvider::GetPassKey())
          .ToPrefString());
}

}  // namespace glic
