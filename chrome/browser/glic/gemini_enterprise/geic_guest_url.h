// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_GUEST_URL_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_GUEST_URL_H_

#include <string_view>

#include "base/types/expected.h"
#include "url/gurl.h"

namespace geic {

// Configuration source that produced a `GeicGuestUrl`.
enum class GuestUrlSource {
  kSwitch,
  kPolicy,
  kFinch,
};

// Reason a guest URL source did not produce a usable `GeicGuestUrl`.
enum class GuestUrlError {
  // The source is not set.
  kUnset,
  // The source is set, but to an unparsable or disallowed URL.
  kInvalid,
};

// A validated and canonicalized GEiC guest URL. Instances are created by the
// per-source factories below, so holders know `url()` passed its source's
// checks.
class GeicGuestUrl {
 public:
  static base::expected<GeicGuestUrl, GuestUrlError> FromSwitch(
      std::string_view spec,
      bool allow_any_http_url);
  static base::expected<GeicGuestUrl, GuestUrlError> FromPolicy(
      std::string_view spec);
  static base::expected<GeicGuestUrl, GuestUrlError> FromFinch(
      std::string_view spec);

  GeicGuestUrl(const GeicGuestUrl&);
  GeicGuestUrl& operator=(const GeicGuestUrl&);
  GeicGuestUrl(GeicGuestUrl&&) noexcept;
  GeicGuestUrl& operator=(GeicGuestUrl&&) noexcept;
  ~GeicGuestUrl();

  // Two instances are equal when both `url()` and `source()` match.
  friend bool operator==(const GeicGuestUrl&, const GeicGuestUrl&) = default;

  // The canonicalized guest URL loaded in the side panel, with `idp` appended
  // when unwrapped from a Workforce Auth Portal URL.
  const GURL& url() const { return url_; }
  GuestUrlSource source() const { return source_; }

  // Returns the Gemini Enterprise web app's Skills page URL for `url()`.
  GURL SkillsUrl() const;

 private:
  GeicGuestUrl(GURL url, GuestUrlSource source);

  GURL url_;
  GuestUrlSource source_;
};

}  // namespace geic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GEIC_GUEST_URL_H_
