// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "url/gurl.h"

class Profile;

namespace glic {

// LINT.IfChange(AiSettingsSection)
inline constexpr char kGeminiSettingsSectionId[] = "gemini";
inline constexpr char kGeminiEnterpriseSettingsSectionId[] = "geminiEnterprise";
// LINT.ThenChange(//chrome/browser/resources/settings/route.ts:AiSettingsSection)

// Guest URL key (`GlicProviderSpec::GetGuestUrlKey()`) of prod consumer Gemini:
// its guest URL host. Gemini falls back to it if its guest URL has no host, and
// v1 client data storage items are migrated to it. Persisted; must not change.
inline constexpr char kGeminiGuestUrlKey[] = "gemini.google.com";

struct GeicProviderInitOptions {
  GURL guest_url;
  bool can_switch_between_gemini_providers = false;
};

struct GlicPdfAutoOpenFeatureParams {
  bool with_onboarding = true;
  base::TimeDelta cooldown = base::Hours(1);
};

struct GlicPdfSummarizeFeatureParams {
  int arm = 3;
  bool use_long_button_text = false;
};

struct GlicSelectionInlineCueFeatureParams {
  static constexpr char kCtaTellMe[] = "tell_me_about_this";
  static constexpr char kCtaExplain[] = "explain";

  GlicSelectionInlineCueFeatureParams();
  GlicSelectionInlineCueFeatureParams(
      const GlicSelectionInlineCueFeatureParams&);
  GlicSelectionInlineCueFeatureParams& operator=(
      const GlicSelectionInlineCueFeatureParams&);
  GlicSelectionInlineCueFeatureParams(GlicSelectionInlineCueFeatureParams&&);
  GlicSelectionInlineCueFeatureParams& operator=(
      GlicSelectionInlineCueFeatureParams&&);
  ~GlicSelectionInlineCueFeatureParams();

  bool show_copy_buttons = false;
  bool auto_send_prompt = true;
  std::string cta = kCtaExplain;
  base::flat_set<std::string> default_blocked_sites;
};

struct GlicIndigoFeatureParams {
  GlicIndigoFeatureParams();
  GlicIndigoFeatureParams(const GlicIndigoFeatureParams&);
  GlicIndigoFeatureParams& operator=(const GlicIndigoFeatureParams&);
  GlicIndigoFeatureParams(GlicIndigoFeatureParams&&);
  GlicIndigoFeatureParams& operator=(GlicIndigoFeatureParams&&);
  ~GlicIndigoFeatureParams();

  // Prompt sent to Glic. Overrides `prompt_key`.
  std::string prompt;
  // Key of a prompt from the Indigo component, used if `prompt` is empty.
  std::string prompt_key;
  // Skill whose prompt is sent to Glic instead, if the skill exists.
  std::string skill_id;
  // Delay between the Glic panel opening and the Indigo agent running.
  base::TimeDelta trigger_delay = base::Milliseconds(300);
};

struct GlicMarketingAutoOpenFeatureParams {
  GlicMarketingAutoOpenFeatureParams();
  GlicMarketingAutoOpenFeatureParams(const GlicMarketingAutoOpenFeatureParams&);
  GlicMarketingAutoOpenFeatureParams& operator=(
      const GlicMarketingAutoOpenFeatureParams&);
  GlicMarketingAutoOpenFeatureParams(GlicMarketingAutoOpenFeatureParams&&);
  GlicMarketingAutoOpenFeatureParams& operator=(
      GlicMarketingAutoOpenFeatureParams&&);
  ~GlicMarketingAutoOpenFeatureParams();

  // Comma-separated URLs that auto-open Glic. Query and fragment are ignored
  // when matching.
  std::string allowlisted_urls;
  // Maximum number of auto-opens per profile.
  int max_impressions = 1;
};

class GlicFreStore;
// Provider-specific data for a profile's Glic surface. Feature code reads
// values from here instead of checking which provider the profile uses.
// Holds data only. Values don't change for a given profile.
class GlicProviderSpec;

// Returns the `GlicProviderSpec` for `profile` owned by `GlicProviderLatch`
// (or a static Consumer spec when `profile` is null or not latched).
// Off-the-record profiles use the spec of their original profile.
const GlicProviderSpec& GetGlicProviderSpec(Profile* profile);

// Creates the per-profile `GlicProviderSpec` stored in `GlicProviderLatch`.
std::unique_ptr<const GlicProviderSpec> CreateGlicProviderSpec(
    GlicProvider provider,
    GeicProviderInitOptions geic_options);

class GlicProviderSpec {
 public:
  GlicProviderSpec() = default;
  GlicProviderSpec(const GlicProviderSpec&) = delete;
  GlicProviderSpec& operator=(const GlicProviderSpec&) = delete;
  virtual ~GlicProviderSpec() = default;

  // Id of the AI settings page section for this provider, sent to the settings
  // WebUI as "aiSettingsSection" (e.g. "gemini"). Must match AiSettingsSection
  // in chrome/browser/resources/settings/route.ts.
  virtual std::string_view GetSettingsSectionId() const = 0;
  // The settings subpage that OpenGlicSettingsPage opens, e.g. "ai/gemini".
  virtual std::string_view GetSettingsSubpage() const = 0;
  // Whether the settings UI for this provider should offer the control to
  // switch between Gemini Enterprise and consumer Gemini.
  virtual bool CanSwitchBetweenGeminiProviders() const = 0;
  // Resource ID of the entry-point (Glic button) icon.
  virtual int GetEntryPointIconId() const = 0;
  // Resource ID of the Glic IPH promo body string.
  virtual int GetPromoBodyStringId() const = 0;
  // The synthetic field trial group name for this provider.
  virtual std::string_view GetSyntheticTrialGroupName() const = 0;
  // Infix for per-provider metric names: Glic.<Infix>.X.
  virtual std::string_view GetMetricsInfix() const = 0;
  // Whether browser web actuation (Actor) is supported for this provider.
  virtual bool IsActuationSupported() const = 0;
  // Whether this provider requires NoWebview mode.
  virtual bool RequiresNoWebview() const = 0;
  // Returns the localized guest URL to load for `profile`, or an empty GURL
  // if none is configured.
  virtual GURL GetGuestUrl(Profile* profile) const = 0;
  // Whether the entry point stays visible, in an error state, for onboarded
  // profiles that fail a Glic requirement
  // (`features::kGlicAnchorEntryPointForOnboardedUsers`).
  virtual bool SupportsAnchoredEntryPoint() const = 0;
  // The store for the FRE this provider uses for consent.
  // TODO(b/576047045): Bind the FRE store to its profile so its methods no
  // longer need a `Profile*`. Each profile's spec is owned by
  // `GlicProviderLatch`, but `CreateGlicProviderSpec()` doesn't take the
  // profile, and unlatched profiles share a fallback spec.
  virtual const GlicFreStore& GetFreStore() const = 0;
  // Key for prefs that are kept per guest URL, e.g. the web client's client
  // data storage. Gemini: the guest URL host, or `kGeminiGuestUrlKey` if it has
  // none. Gemini Enterprise: the latched guest URL without `hl`.
  // Persisted; must not change.
  virtual std::string GetGuestUrlKey() const = 0;
  // Whether the web client's data in the webview partition's localStorage is
  // copied into `GlicClientDataStorage` (see `MaybeMigrateGlicLocalStorage()`).
  // Only true for providers whose web client was ever loaded in the webview.
  virtual bool MigratesWebviewLocalStorage() const = 0;
  // Whether enablement applies the Gemini account eligibility checks, i.e.
  // whether the profile's consumer Google account may use Gemini in Chrome:
  // the rollout (including subscription tiers), the primary account's
  // sign-in state and capabilities, the GeminiSettings policy and the
  // server-side user status. This is separate from profile eligibility
  // (GlicEnabling::IsProfileEligible()), which applies to every provider.
  //
  // Gemini Enterprise doesn't apply them: its access is decided elsewhere. A
  // profile is only latched to Gemini Enterprise if the GEiC feature is
  // enabled and a Gemini Enterprise guest URL is configured when the profile
  // is created, normally by the admin through the GeminiEnterpriseUrl policy
  // (see geic::ResolveGeicGuestUrl() and GlicProviderLatchFactory), and the
  // Gemini Enterprise web app handles the user's sign-in and access. The
  // checks shared by all providers (the Glic feature, country, locale,
  // system and OS requirements and FRE consent) still apply.
  virtual bool RequiresGeminiAccountEligibility() const = 0;
  // Returns whether `feature` is enabled for the profile if the provider
  // supports it, otherwise false. For the consumer provider, this checks the
  // feature's flag(s), if it has any. Callers still apply profile requirements
  // (e.g. Glic enablement or FRE consent).
  virtual bool SupportsFeature(GlicFeature feature) const = 0;
  virtual GlicPdfAutoOpenFeatureParams GetPdfAutoOpenFeatureParams() const = 0;
  virtual GlicPdfSummarizeFeatureParams GetPdfSummarizeFeatureParams()
      const = 0;
  virtual GlicSelectionInlineCueFeatureParams
  GetSelectionInlineCueFeatureParams() const = 0;
  virtual GlicIndigoFeatureParams GetIndigoFeatureParams() const = 0;
  virtual GlicMarketingAutoOpenFeatureParams GetMarketingAutoOpenFeatureParams()
      const = 0;
  // Whether this provider's guest supports server-side rendering.
  virtual bool SupportsSsr() const = 0;
  // Whether daisy-chained tabs (e.g. tabs opened via CreateTab or link clicks)
  // are automatically pinned as shared context when bound to a Glic instance.
  virtual bool PinOnBindEnabled() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_
