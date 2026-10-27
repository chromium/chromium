// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chrome/browser/glic/glic_fre_store_impl.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_fre_store.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace glic {

namespace {

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
  ~GeminiGlicProviderSpec() override = default;
  GlicSettingsSection GetSettingsSection() const override {
    return GlicSettingsSection::kGemini;
  }
  std::string_view GetSettingsSubpage() const override {
    return chrome::kGlicSettingsSubpage;
  }
  int GetEntryPointIconId() const override { return IDR_GLIC_BUTTON_ALT_ICON; }
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
  bool RequiresGeminiAccountEligibility() const override { return true; }
  bool SupportsFeature(GlicFeature feature) const override { return true; }

 private:
  const std::unique_ptr<GlicFreStore> fre_store_ = CreateGeminiFreStore();
};

class GeminiEnterpriseGlicProviderSpec final : public GlicProviderSpec {
 public:
  explicit GeminiEnterpriseGlicProviderSpec(GeicProviderInitOptions options)
      : guest_url_(std::move(options.guest_url)) {}
  ~GeminiEnterpriseGlicProviderSpec() override = default;
  GlicSettingsSection GetSettingsSection() const override {
    return GlicSettingsSection::kGeminiEnterprise;
  }
  std::string_view GetSettingsSubpage() const override {
    return chrome::kGlicEnterpriseSettingsSubpage;
  }
  int GetEntryPointIconId() const override { return IDR_GEIC_BUTTON_ICON; }
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
  bool RequiresGeminiAccountEligibility() const override { return false; }
  bool SupportsFeature(GlicFeature feature) const override {
    switch (feature) {
      case GlicFeature::kLive:
      case GlicFeature::kShareImage:
        return false;
    }
    NOTREACHED();
  }

 private:
  const GURL guest_url_;
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
      return std::make_unique<GeminiGlicProviderSpec>();
    case GlicProvider::kGeminiEnterprise:
      return std::make_unique<GeminiEnterpriseGlicProviderSpec>(
          std::move(geic_options));
  }
}

}  // namespace glic
