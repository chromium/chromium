// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "net/base/schemeful_site.h"

// Service which encapsulates logic related to displaying and controlling the
// users Privacy Sandbox settings. This service contains the chrome/ specific
// logic used by the UI, including decision making around what the users'
// Privacy Sandbox settings should be based on their existing settings.
// Ultimately the decisions made by this service are consumed (through
// preferences and content settings) by the PrivacySandboxSettings located in
// components/privacy_sandbox/, which in turn makes them available to Privacy
// Sandbox APIs.
class PrivacySandboxService : public KeyedService {
 public:
  // If set to true, this treats the testing environment as that of a branded
  // Chrome build.
  virtual void ForceChromeBuildForTests(bool force_chrome_build) = 0;

  // Toggles the RelatedWebsiteSets preference.
  virtual void SetRelatedWebsiteSetsDataAccessEnabled(bool enabled) = 0;

  // Returns whether the RelatedWebsiteSets preference is enabled.
  virtual bool IsRelatedWebsiteSetsDataAccessEnabled() const = 0;

  // Returns whether the RelatedWebsiteSets preference is managed.
  virtual bool IsRelatedWebsiteSetsDataAccessManaged() const = 0;

  // Returns the owner domain of the related website set that `site_url` is a
  // member of, or std::nullopt if `site_url` is not recognised as a member of
  // an RWS. Encapsulates logic about whether RWS information should be shown,
  // if it should not, std::nullopt is always returned. Virtual for mocking in
  // tests.
  virtual std::optional<net::SchemefulSite> GetRelatedWebsiteSetOwner(
      const GURL& site_url) const = 0;

  // Same as GetRelatedWebsiteSetOwner but returns a formatted string.
  virtual std::optional<std::u16string> GetRelatedWebsiteSetOwnerForDisplay(
      const GURL& site_url) const = 0;

  // Returns true if `site`'s membership in an RWS is being managed by policy or
  // if RelatedWebsiteSets preference is managed. Virtual for mocking in tests.
  //
  // Note: Enterprises can use the Related Website Set Overrides policy to
  // either add or remove a site from a Related Website Set. This method returns
  // true only if `site` is being added into a Related Website Set since there's
  // no UI use for whether `site` is being removed by an enterprise yet.
  virtual bool IsPartOfManagedRelatedWebsiteSet(
      const net::SchemefulSite& site) const = 0;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_PRIVACY_SANDBOX_SERVICE_H_
