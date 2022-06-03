// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_DIGITAL_ASSET_LINKS_CROSS_VALIDATOR_H_
#define CHROME_BROWSER_LOOKALIKES_DIGITAL_ASSET_LINKS_CROSS_VALIDATOR_H_

#include "base/bind.h"
#include "base/time/time.h"
#include "components/digital_asset_links/digital_asset_links_handler.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

namespace base {
class Clock;
}

// Fetches and validates Digital Asset Links manifests from the lookalike and
// target sites.
// |lookalike_domain| is the lookalike domain, |target_domain| is the domain
// that the lookalike domain matched to.
// Runs |callback| with true if |lookalike_domain|'s manifest contains a
// valid entry for |target_domain| AND |target_domain|'s manifest
// contains a valid entry for |lookalike_domain|. Runs |callback| with false in
// all other cases. |timeout| is the total timeout to fetch both manifests. If
// either of the manifest fetches fails during this timeout, the validation is
// assumed to fail.
class DigitalAssetLinkCrossValidator {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Event {
    kNone = 0,
    // A digital asset link validation started. Recorded once for a lookalike
    // and target pair.
    kStarted = 1,
    // Failed to fetch the lookalike manifest. This could be because the
    // lookalike site doesn't serve a manifest, serves an invalid manifest or a
    // manifest that doesn't match the target site.
    kLookalikeManifestFailed = 2,
    // Timed out while fetching the lookalike site's manifest.
    kLookalikeManifestTimedOut = 3,
    // Failed to fetch the target manifest. This could be because the
    // target site doesn't serve a manifest, serves an invalid manifest or a
    // manifest that doesn't match the lookalike site.
    kTargetManifestFailed = 4,
    // Timed out while fetching the target site's manifest.
    kTargetManifestTimedOut = 5,
    // Validation of lookalike and target manifests succeeded, no need to show
    // a lookalike warning.
    kValidationSucceeded = 6,
    kMaxValue = kValidationSucceeded
  };
  DigitalAssetLinkCrossValidator(Profile* profile,
                                 const url::Origin& lookalike_domain,
                                 const url::Origin& target_domain,
                                 base::TimeDelta timeout,
                                 base::Clock* clock,
                                 ResultCallback callback);

  ~DigitalAssetLinkCrossValidator();

  void Start();

  static const char kEventHistogramName[];

 private:
  void OnFetchLookalikeManifestComplete(
      digital_asset_links::RelationshipCheckResult result);
  void OnFetchTargetManifestComplete(
      digital_asset_links::RelationshipCheckResult result);

  const url::Origin lookalike_domain_;
  const url::Origin target_domain_;
  const base::TimeDelta timeout_;
  base::TimeDelta target_manifest_timeout_;
  base::Time start_time_;
  const base::Clock* clock_;
  ResultCallback callback_;

  std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler>
      asset_link_handler_;
};

#endif  // CHROME_BROWSER_LOOKALIKES_DIGITAL_ASSET_LINKS_CROSS_VALIDATOR_H_
