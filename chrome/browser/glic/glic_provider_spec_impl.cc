// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_provider_spec_impl.h"

#include <memory>
#include <string_view>

#include "base/no_destructor.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"

namespace glic {

namespace {

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
};

class GeminiEnterpriseGlicProviderSpec final : public GlicProviderSpec {
 public:
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
  bool IsActuationSupported() const override { return false; }
  bool RequiresNoWebview() const override { return true; }
};

}  // namespace

const GlicProviderSpec& GetGlicProviderSpec(Profile* profile) {
  if (profile) {
    if (auto* service = GlicKeyedService::Get(profile)) {
      return service->provider_spec();
    }
    if (profile->IsRegularProfile() && geic::IsGeicEnabled(profile)) {
      static base::NoDestructor<GeminiEnterpriseGlicProviderSpec>
          enterprise_spec;
      return *enterprise_spec;
    }
  }
  static base::NoDestructor<GeminiGlicProviderSpec> fallback_spec;
  return *fallback_spec;
}

std::unique_ptr<GlicProviderSpec> CreateGlicProviderSpec(Profile* profile) {
  return geic::IsGeicEnabled(profile)
             ? std::unique_ptr<GlicProviderSpec>(
                   std::make_unique<GeminiEnterpriseGlicProviderSpec>())
             : std::make_unique<GeminiGlicProviderSpec>();
}

}  // namespace glic
