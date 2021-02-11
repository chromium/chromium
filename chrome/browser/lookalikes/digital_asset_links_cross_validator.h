// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_DIGITAL_ASSET_LINKS_CROSS_VALIDATOR_H_
#define CHROME_BROWSER_LOOKALIKES_DIGITAL_ASSET_LINKS_CROSS_VALIDATOR_H_

#include "base/bind.h"
#include "chrome/browser/installable/digital_asset_links/digital_asset_links_handler.h"
#include "url/gurl.h"
#include "url/origin.h"

class Profile;

// Fetches and validates Digital Asset Links manifests from the lookalike and
// target sites.
// |lookalike_domain| is the lookalike domain, |target_domain| is the domain
// that the lookalike domain matched to.
// Runs |callback| with true if |lookalike_domain|'s manifest contains a
// valid entry for |target_domain| AND |target_domain|'s manifest
// contains a valid entry for |lookalike_domain|. Runs |callback| with false in
// all other cases.
class DigitalAssetLinkCrossValidator {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;

  DigitalAssetLinkCrossValidator(Profile* profile,
                                 const url::Origin& lookalike_domain,
                                 const url::Origin& target_domain,
                                 ResultCallback callback);
  ~DigitalAssetLinkCrossValidator();

  void Start();

 private:
  void OnFetchLookalikeManifestComplete(
      digital_asset_links::RelationshipCheckResult result);
  void OnFetchTargetManifestComplete(
      digital_asset_links::RelationshipCheckResult result);

  const url::Origin lookalike_domain_;
  const url::Origin target_domain_;
  ResultCallback callback_;

  std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler>
      asset_link_handler_;
};

#endif  // CHROME_BROWSER_LOOKALIKES_DIGITAL_ASSET_LINKS_CROSS_VALIDATOR_H_
