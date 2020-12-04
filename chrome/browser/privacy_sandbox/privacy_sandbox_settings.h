// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cookies/cookie_constants.h"

class PrefService;

namespace content_settings {
class CookieSettings;
}

namespace url {
class Origin;
}

// A service which acts as a intermediary between Privacy Sandbox APIs and the
// preferences and content settings which define when they are allowed to be
// accessed.
// TODO (crbug.com/1154686): Move this and other Privacy Sandbox items into
// components.
class PrivacySandboxSettings : public KeyedService {
 public:
  PrivacySandboxSettings(content_settings::CookieSettings* cookie_settings,
                         PrefService* prefs);

  // Determines whether FLoC is allowable in a particular context.
  // |top_frame_origin| is used to check for content settings which could both
  // affect 1P and 3P contexts.
  bool IsFlocAllowed(const GURL& url,
                     const base::Optional<url::Origin>& top_frame_origin) const;

  // Returns the point in time from which history is elligible to be used when
  // calculating a user's FLoC ID. Reset when a user clears all cookies, or
  // when the browser restarts with "Clear on exit" enabled. The returned time
  // will have been fuzzed for local privacy, and so may be in the future, in
  // which case no history is elligible.
  base::Time FlocDataAccessibleSince() const;

  // Determines whether Conversion Measurement is allowable in a particular
  // context. Should be called at both impression & conversion. At each of these
  // points |top_frame_origin| is the same as either the impression origin or
  // the conversion origin respectively.
  bool IsConversionMeasurementAllowed(
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin) const;

  // Called before sending the associated conversion report to
  // |reporting_origin|. Re-checks that |reporting_origin| is allowable as a 3P
  // on both |impression_origin| and |conversion_origin|.
  bool ShouldSendConversionReport(const url::Origin& impression_origin,
                                  const url::Origin& conversion_origin,
                                  const url::Origin& reporting_origin) const;

 private:
  content_settings::CookieSettings* cookie_settings_;
  PrefService* pref_service_;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SETTINGS_H_
