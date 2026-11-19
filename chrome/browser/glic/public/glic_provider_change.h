// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_CHANGE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_CHANGE_H_

#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "chrome/browser/glic/public/glic_provider.h"

namespace glic {

// How the provider that the current configuration resolves to differs from the
// provider latched for a profile. The latch is fixed for the profile's
// lifetime, so a change only takes effect after a relaunch, and a change that
// is reverted before then goes back to `kNone`.
enum class GlicProviderChange {
  kNone,
  // Gemini -> GEiC.
  kGeminiToGeic,
  // GEiC -> Gemini, e.g. the policy was removed or set to an invalid URL.
  kGeicToGemini,
  // GEiC A -> GEiC B, i.e. a different guest URL.
  kGeicToOtherGeic,
};

// Identifies a provider for detecting provider changes: two
// `GlicProviderSpec::GetProviderKey()`s are equal iff they're the same
// provider. Gemini Enterprise providers with different guest URL keys are
// different providers.
class GlicProviderKey {
 public:
  static GlicProviderKey Gemini() { return GlicProviderKey(std::string()); }
  // `guest_url_key` is the provider's `GlicProviderSpec::GetGuestUrlKey()`.
  static GlicProviderKey GeminiEnterprise(std::string guest_url_key) {
    CHECK(!guest_url_key.empty());
    return GlicProviderKey(std::move(guest_url_key));
  }
  // Converts from and to the persisted form: "" for Gemini, else the Gemini
  // Enterprise guest URL key.
  static GlicProviderKey FromPrefString(std::string value) {
    return GlicProviderKey(std::move(value));
  }
  const std::string& ToPrefString() const { return guest_url_key_; }

  GlicProvider provider() const {
    return guest_url_key_.empty() ? GlicProvider::kGemini
                                  : GlicProvider::kGeminiEnterprise;
  }

  friend bool operator==(const GlicProviderKey&,
                         const GlicProviderKey&) = default;
  friend std::ostream& operator<<(std::ostream& os,
                                  const GlicProviderKey& key) {
    return key.provider() == GlicProvider::kGemini
               ? os << "Gemini"
               : os << "GeminiEnterprise(" << key.guest_url_key_ << ")";
  }

 private:
  explicit GlicProviderKey(std::string guest_url_key)
      : guest_url_key_(std::move(guest_url_key)) {}

  // Empty iff Gemini.
  std::string guest_url_key_;
};

// Classifies the change from the `from` provider to the `to` provider.
inline GlicProviderChange ClassifyProviderChange(const GlicProviderKey& from,
                                                 const GlicProviderKey& to) {
  if (from == to) {
    return GlicProviderChange::kNone;
  }
  if (from.provider() == GlicProvider::kGemini) {
    return GlicProviderChange::kGeminiToGeic;
  }
  return to.provider() == GlicProvider::kGemini
             ? GlicProviderChange::kGeicToGemini
             : GlicProviderChange::kGeicToOtherGeic;
}

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PROVIDER_CHANGE_H_
