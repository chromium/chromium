// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_fre_store_impl.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_fre_store.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/features.h"
#include "net/base/url_util.h"
#include "pdf/buildflags.h"
#include "url/gurl.h"

namespace glic {

GlicIndigoFeatureParams::GlicIndigoFeatureParams() = default;
GlicIndigoFeatureParams::GlicIndigoFeatureParams(
    const GlicIndigoFeatureParams&) = default;
GlicIndigoFeatureParams& GlicIndigoFeatureParams::operator=(
    const GlicIndigoFeatureParams&) = default;
GlicIndigoFeatureParams::GlicIndigoFeatureParams(GlicIndigoFeatureParams&&) =
    default;
GlicIndigoFeatureParams& GlicIndigoFeatureParams::operator=(
    GlicIndigoFeatureParams&&) = default;
GlicIndigoFeatureParams::~GlicIndigoFeatureParams() = default;

GlicMarketingAutoOpenFeatureParams::GlicMarketingAutoOpenFeatureParams() =
    default;
GlicMarketingAutoOpenFeatureParams::GlicMarketingAutoOpenFeatureParams(
    const GlicMarketingAutoOpenFeatureParams&) = default;
GlicMarketingAutoOpenFeatureParams&
GlicMarketingAutoOpenFeatureParams::operator=(
    const GlicMarketingAutoOpenFeatureParams&) = default;
GlicMarketingAutoOpenFeatureParams::GlicMarketingAutoOpenFeatureParams(
    GlicMarketingAutoOpenFeatureParams&&) = default;
GlicMarketingAutoOpenFeatureParams&
GlicMarketingAutoOpenFeatureParams::operator=(
    GlicMarketingAutoOpenFeatureParams&&) = default;
GlicMarketingAutoOpenFeatureParams::~GlicMarketingAutoOpenFeatureParams() =
    default;

namespace {

// Appended by `GetLocalizedGuestURL()` from Chrome's UI locale.
constexpr char kLocaleParam[] = "hl";

// Returns `guest_url` without `hl`, so the key survives UI language changes.
// Other params may identify the GE instance, so they are kept. Persisted; must
// not change.
std::string GuestUrlToKey(const GURL& guest_url) {
  GURL key =
      net::AppendOrReplaceQueryParameter(guest_url, kLocaleParam, std::nullopt);
  if (key.query().empty()) {
    // AppendOrReplaceQueryParameter() always sets a query, so drop the empty
    // "?" it leaves when no other params remain.
    GURL::Replacements replacements;
    replacements.ClearQuery();
    key = key.ReplaceComponents(replacements);
  }
  return key.spec();
}

GURL MaybeAppendAuthUser(const GURL& guest_url, Profile* profile) {
  if (!profile) {
    return guest_url;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicSetAuthUser) ||
      !GlicKeyedService::IsNoWebviewEnabled(profile)) {
    return guest_url;
  }
  if (!guest_url.is_valid()) {
    return GURL();
  }
  std::string unused_output;
  if (net::GetValueForKeyInQuery(guest_url, "authuser", &unused_output)) {
    return guest_url;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return guest_url;
  }
  std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  if (email.empty()) {
    return guest_url;
  }
  if (identity_manager->GetAccountsInCookieJar().AreAccountsFresh()) {
    std::optional<size_t> session_index =
        identity_manager->GetSessionIndexForPrimaryAccount();
    if (session_index && *session_index == 0) {
      return guest_url;
    }
  }
  return net::AppendQueryParameter(guest_url, "authuser", email);
}

class GeminiGlicProviderSpec final : public GlicProviderSpec {
 public:
  explicit GeminiGlicProviderSpec(GeicProviderInitOptions options = {})
      : can_switch_between_gemini_providers_(
            options.can_switch_between_gemini_providers) {}
  ~GeminiGlicProviderSpec() override = default;
  std::string_view GetSettingsSectionId() const override {
    return kGeminiSettingsSectionId;
  }
  std::string_view GetSettingsSubpage() const override {
    return chrome::kGlicSettingsSubpage;
  }
  bool CanSwitchBetweenGeminiProviders() const override {
    return can_switch_between_gemini_providers_;
  }
  int GetEntryPointIconId() const override { return IDR_GLIC_BUTTON_ALT_ICON; }
  int GetPromoBodyStringId() const override { return IDS_GLIC_PROMO_BODY; }
  std::string_view GetSyntheticTrialGroupName() const override {
    return "Gemini";
  }
  std::string_view GetMetricsInfix() const override { return "Consumer"; }
  bool IsActuationSupported() const override { return true; }
  bool RequiresNoWebview() const override { return false; }
  GURL GetGuestUrl(Profile* profile) const override {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    bool has_glic_guest_url =
        command_line->HasSwitch(::switches::kGlicGuestURL);
    GURL url =
        GURL(has_glic_guest_url
                 ? command_line->GetSwitchValueASCII(::switches::kGlicGuestURL)
                 : features::kGlicGuestURL.Get());
    url = MaybeApplyPresetGuestUrl(std::move(url));
    if (url.is_empty()) {
      LOG(ERROR) << "No glic guest url";
      return GURL();
    }
    return GetLocalizedGuestURL(MaybeAppendAuthUser(url, profile));
  }
  bool SupportsAnchoredEntryPoint() const override { return true; }
  const GlicFreStore& GetFreStore() const override { return *fre_store_; }
  std::string GetGuestUrlKey() const override {
    GURL guest_url = GetGuestUrl(/*profile=*/nullptr);
    // Only an invalid guest URL flag or preset has no host. The web client
    // can't load then, so nothing is stored under the fallback.
    if (!guest_url.is_valid() || guest_url.host().empty()) {
      return kGeminiGuestUrlKey;
    }
    return std::string(guest_url.host());
  }
  bool MigratesWebviewLocalStorage() const override { return true; }
  bool RequiresGeminiAccountEligibility() const override { return true; }
  bool SupportsFeature(GlicFeature feature) const override {
    switch (feature) {
      case GlicFeature::kLive:
        // TODO(crbug.com/578498098): Return features::kGlicLiveMode once
        // callers that skip the flag are updated.
        return true;
      case GlicFeature::kShareImage:
        // TODO(crbug.com/578498098): Return features::kGlicShareImage once
        // share_image_allowed checks the flag.
        return true;
      case GlicFeature::kOsEntryPoints:
        // No flag: OS entry points are controlled by prefs.
        return true;
      case GlicFeature::kExperimentalTriggering:
        return base::FeatureList::IsEnabled(
            features::kGlicExperimentalTriggering);
      case GlicFeature::kPdfAutoOpen:
        return base::FeatureList::IsEnabled(features::kAutoOpenGlicForPdf);
      case GlicFeature::kPdfSummarize:
#if BUILDFLAG(ENABLE_PDF)
        return base::FeatureList::IsEnabled(features::kPdfGlicSummarize);
#else
        return false;
#endif  // BUILDFLAG(ENABLE_PDF)
      case GlicFeature::kSelectionInlineCue:
        return base::FeatureList::IsEnabled(features::kGlicSelectionPrompt);
      case GlicFeature::kCaptureRegion:
        return base::FeatureList::IsEnabled(features::kGlicCaptureRegion);
      case GlicFeature::kFirstRunShowcase:
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        return base::FeatureList::IsEnabled(
            features::kFirstRunFeatureShowcaseGeminiStep);
#else
        return false;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      // Callers apply the flags: the contextual cueing helpers, glicPrivate
      // (per-source `extensions_features` flags) and the web continuity
      // navigation throttle.
      case GlicFeature::kContextualCueing:
      case GlicFeature::kZeroStateSuggestions:
      case GlicFeature::kGooglePageInvocation:
      case GlicFeature::kWebContinuity:
      case GlicFeature::kAutofill:
      case GlicFeature::kAiOverlay:
      case GlicFeature::kPersonalContext:
        return true;
      case GlicFeature::kSkills:
        return base::FeatureList::IsEnabled(::features::kSkillsEnabled);
      case GlicFeature::kMarketingAutoOpen:
        return base::FeatureList::IsEnabled(features::kGlicMarketingAutoOpen);
      case GlicFeature::kIndigoOpenGlic:
        return base::FeatureList::IsEnabled(features::kIndigoOpenGlic);
      case GlicFeature::kDynamicChromeTools:
        return base::FeatureList::IsEnabled(features::kGlicDynamicChromeTools);
    }
    NOTREACHED();
  }
  GlicExperimentalTriggeringFeatureParams
  GetExperimentalTriggeringFeatureParams() const override {
    return features::PopulateExperimentalTriggeringFeatureParams(
        features::kGlicExperimentalTriggering);
  }
  GlicPdfAutoOpenFeatureParams GetPdfAutoOpenFeatureParams() const override {
    return features::PopulatePdfAutoOpenFeatureParams(
        features::kAutoOpenGlicForPdf);
  }
  GlicPdfSummarizeFeatureParams GetPdfSummarizeFeatureParams() const override {
#if BUILDFLAG(ENABLE_PDF)
    return features::PopulatePdfSummarizeFeatureParams(
        features::kPdfGlicSummarize);
#else
    return {};
#endif  // BUILDFLAG(ENABLE_PDF)
  }
  GlicSelectionInlineCueFeatureParams GetSelectionInlineCueFeatureParams()
      const override {
    return features::PopulateSelectionInlineCueFeatureParams(
        features::kGlicSelectionPrompt);
  }
  GlicIndigoFeatureParams GetIndigoFeatureParams() const override {
    return features::PopulateIndigoFeatureParams(features::kIndigoOpenGlic);
  }
  GlicMarketingAutoOpenFeatureParams GetMarketingAutoOpenFeatureParams()
      const override {
    return features::PopulateMarketingAutoOpenFeatureParams(
        features::kGlicMarketingAutoOpen);
  }
  bool SupportsSsr() const override {
    return base::FeatureList::IsEnabled(features::kGlicSsr);
  }
  bool PinOnBindEnabled() const override { return true; }

 private:
  const bool can_switch_between_gemini_providers_;
  const std::unique_ptr<GlicFreStore> fre_store_ = CreateGeminiFreStore();
};

class GeminiEnterpriseGlicProviderSpec final : public GlicProviderSpec {
 public:
  explicit GeminiEnterpriseGlicProviderSpec(GeicProviderInitOptions options)
      : guest_url_(std::move(options.guest_url)),
        can_switch_between_gemini_providers_(
            options.can_switch_between_gemini_providers) {}
  ~GeminiEnterpriseGlicProviderSpec() override = default;
  std::string_view GetSettingsSectionId() const override {
    return kGeminiEnterpriseSettingsSectionId;
  }
  std::string_view GetSettingsSubpage() const override {
    return chrome::kGlicEnterpriseSettingsSubpage;
  }
  bool CanSwitchBetweenGeminiProviders() const override {
    return can_switch_between_gemini_providers_;
  }
  int GetEntryPointIconId() const override { return IDR_GEIC_BUTTON_ICON; }
  int GetPromoBodyStringId() const override { return IDS_GEIC_PROMO_BODY; }
  std::string_view GetSyntheticTrialGroupName() const override {
    return "GeminiEnterprise";
  }
  std::string_view GetMetricsInfix() const override { return "Enterprise"; }
  bool IsActuationSupported() const override {
    return base::FeatureList::IsEnabled(features::kGeicActuation);
  }
  bool RequiresNoWebview() const override { return true; }
  GURL GetGuestUrl(Profile* profile) const override {
    if (guest_url_.is_empty()) {
      VLOG(1) << "GEiC is enabled but no guest URL was provided.";
      return GURL();
    }
    return GetLocalizedGuestURL(guest_url_);
  }
  // The anchored error state shows consumer copy and help links.
  // TODO(b/576834093): Support anchoring with GEiC error states.
  bool SupportsAnchoredEntryPoint() const override { return false; }
  const GlicFreStore& GetFreStore() const override { return *fre_store_; }
  std::string GetGuestUrlKey() const override {
    // GE profiles are only latched with a valid guest URL (see
    // `GlicProviderLatchFactory`).
    CHECK(guest_url_.is_valid());
    return GuestUrlToKey(guest_url_);
  }
  // GEiC only loads in NoWebview mode, so it never used the webview partition.
  bool MigratesWebviewLocalStorage() const override { return false; }
  bool RequiresGeminiAccountEligibility() const override { return false; }
  bool SupportsFeature(GlicFeature feature) const override {
    switch (feature) {
      case GlicFeature::kLive:
      case GlicFeature::kShareImage:
      case GlicFeature::kOsEntryPoints:
      case GlicFeature::kExperimentalTriggering:
      case GlicFeature::kPdfAutoOpen:
      case GlicFeature::kPdfSummarize:
      case GlicFeature::kSelectionInlineCue:
      case GlicFeature::kCaptureRegion:
      case GlicFeature::kFirstRunShowcase:
      case GlicFeature::kContextualCueing:
      case GlicFeature::kZeroStateSuggestions:
      case GlicFeature::kSkills:
      case GlicFeature::kGooglePageInvocation:
      case GlicFeature::kMarketingAutoOpen:
      case GlicFeature::kWebContinuity:
      case GlicFeature::kIndigoOpenGlic:
      case GlicFeature::kAutofill:
      case GlicFeature::kAiOverlay:
      case GlicFeature::kDynamicChromeTools:
      case GlicFeature::kPersonalContext:
        return false;
    }
    NOTREACHED();
  }
  GlicExperimentalTriggeringFeatureParams
  GetExperimentalTriggeringFeatureParams() const override {
    return {};
  }
  GlicPdfAutoOpenFeatureParams GetPdfAutoOpenFeatureParams() const override {
    return {};
  }
  GlicPdfSummarizeFeatureParams GetPdfSummarizeFeatureParams() const override {
    return {};
  }
  GlicSelectionInlineCueFeatureParams GetSelectionInlineCueFeatureParams()
      const override {
    return {};
  }
  GlicIndigoFeatureParams GetIndigoFeatureParams() const override { return {}; }
  GlicMarketingAutoOpenFeatureParams GetMarketingAutoOpenFeatureParams()
      const override {
    return {};
  }
  // TODO(b/576365433): Enable `kGeicSsr` once the webapp supports it.
  bool SupportsSsr() const override {
    return base::FeatureList::IsEnabled(features::kGeicSsr);
  }
  // GEiC uses explicit single-tab attachment; tabs created via CreateTab or
  // link clicks (e.g. Send feedback, Help, citations) should not be auto-pinned
  // as shared tab context.
  bool PinOnBindEnabled() const override { return false; }

 private:
  const GURL guest_url_;
  const bool can_switch_between_gemini_providers_;
  const std::unique_ptr<GlicFreStore> fre_store_ =
      CreateGeminiEnterpriseFreStore();
};

}  // namespace

const GlicProviderSpec& GetGlicProviderSpec(Profile* profile) {
  if (profile) {
    return GlicProviderLatch::Get(*profile).GetSpec();
  }
  static base::NoDestructor<GeminiGlicProviderSpec> gemini_spec;
  return *gemini_spec;
}

std::unique_ptr<const GlicProviderSpec> CreateGlicProviderSpec(
    GlicProvider provider,
    GeicProviderInitOptions geic_options) {
  switch (provider) {
    case GlicProvider::kGemini:
      return std::make_unique<GeminiGlicProviderSpec>(std::move(geic_options));
    case GlicProvider::kGeminiEnterprise:
      return std::make_unique<GeminiEnterpriseGlicProviderSpec>(
          std::move(geic_options));
  }
}

}  // namespace glic
