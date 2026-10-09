// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_

#include <string_view>

namespace glic {

enum class GlicSettingsSection { kGemini, kGeminiEnterprise };

// Provider-specific data for a profile's Glic surface. Feature code reads
// values from here instead of checking which provider the profile uses.
// Holds data only. Values don't change for a given profile.
class GlicProviderSpec {
 public:
  GlicProviderSpec() = default;
  GlicProviderSpec(const GlicProviderSpec&) = delete;
  GlicProviderSpec& operator=(const GlicProviderSpec&) = delete;
  virtual ~GlicProviderSpec() = default;

  // The AI settings section to show for this provider.
  virtual GlicSettingsSection GetSettingsSection() const = 0;
  // The settings subpage that OpenGlicSettingsPage opens, e.g. "ai/gemini".
  virtual std::string_view GetSettingsSubpage() const = 0;
  // Resource ID of the entry-point (Glic button) icon.
  virtual int GetEntryPointIconId() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_SPEC_H_
