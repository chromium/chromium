// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace geic {

namespace {

using prefs::GeicFreStatus;

// Key of the status in each `kGeicCompletedFre` entry. Persisted; must not
// change.
constexpr char kStatusKey[] = "status";

// Appended by `GetLocalizedGuestURL()` from Chrome's UI locale.
constexpr char kLocaleParam[] = "hl";

// Returns `guest_url` without `hl`, so the key survives UI language changes.
// Other params may identify the GE instance, so they are kept. Persisted.
std::string GuestUrlToFreKey(const GURL& guest_url) {
  GURL key =
      net::AppendOrReplaceQueryParameter(guest_url, kLocaleParam, std::nullopt);
  if (key.query().empty()) {
    // Drop the empty "?" left when `hl` was the only param.
    GURL::Replacements replacements;
    replacements.ClearQuery();
    key = key.ReplaceComponents(replacements);
  }
  return key.spec();
}

// Returns the `kGeicCompletedFre` key for `profile`'s GE instance (see
// `GuestUrlToFreKey()`), shared by all GE accounts in the profile.
// Returns std::nullopt if the GEiC FRE doesn't apply to `profile`, i.e.
// `features::kGeicFre` is disabled or `profile` isn't latched to Gemini
// Enterprise.
// TODO(b/574495404): Move the GEiC FRE applicability check into GLiC's
// per-provider enablement rules.
std::optional<std::string> GetFrePrefKey(Profile* profile) {
  if (!profile || !base::FeatureList::IsEnabled(features::kGeicFre) ||
      glic::GlicProviderLatch::GetProvider(*profile) !=
          glic::GlicProvider::kGeminiEnterprise) {
    return std::nullopt;
  }
  const GURL guest_url =
      glic::GetGlicProviderSpec(profile).GetGuestUrl(profile);
  if (!guest_url.is_valid()) {
    return std::nullopt;
  }
  return GuestUrlToFreKey(guest_url);
}

// Returns the status persisted for `key`, ignoring `--geic-always-skip-fre`.
// Fails closed: returns `kNotStarted` if the entry is missing or invalid.
GeicFreStatus GetStoredFreStatus(Profile* profile, std::string_view key) {
  const base::DictValue* entry =
      profile->GetPrefs()->GetDict(prefs::kGeicCompletedFre).FindDict(key);
  const std::optional<int> raw =
      entry ? entry->FindInt(kStatusKey) : std::nullopt;
  if (!raw || *raw < std::to_underlying(GeicFreStatus::kMinValue) ||
      *raw > std::to_underlying(GeicFreStatus::kMaxValue)) {
    return GeicFreStatus::kNotStarted;
  }
  return static_cast<GeicFreStatus>(*raw);
}

}  // namespace

std::optional<GeicFreStatus> GetFreStatus(Profile* profile) {
  const std::optional<std::string> key = GetFrePrefKey(profile);
  if (!key) {
    return std::nullopt;
  }
  // GEiC counterpart of `--glic-always-skip-fre`. Applied at read time so that
  // nothing is persisted.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kGeicAlwaysSkipFre)) {
    return GeicFreStatus::kCompleted;
  }
  return GetStoredFreStatus(profile, *key);
}

void CompleteFre(Profile* profile) {
  const std::optional<std::string> key = GetFrePrefKey(profile);
  if (!key) {
    return;
  }
  if (GetStoredFreStatus(profile, *key) == GeicFreStatus::kCompleted) {
    return;
  }
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kGeicCompletedFre);
  update->Set(*key,
              base::DictValue().Set(
                  kStatusKey, std::to_underlying(GeicFreStatus::kCompleted)));
}

}  // namespace geic
