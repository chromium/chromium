// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/digital_asset_links_cross_validator.h"

#include "chrome/browser/profiles/profile.h"

namespace {
const char* kDigitalAssetLinkRecordType = "lookalikes/allowlist";
}

DigitalAssetLinkCrossValidator::DigitalAssetLinkCrossValidator(
    Profile* profile,
    const url::Origin& lookalike_domain,
    const url::Origin& target_domain,
    ResultCallback callback)
    : lookalike_domain_(lookalike_domain),
      target_domain_(target_domain),
      callback_(std::move(callback)),
      asset_link_handler_(
          std::make_unique<digital_asset_links::DigitalAssetLinksHandler>(
              profile->GetURLLoaderFactory())) {}

DigitalAssetLinkCrossValidator::~DigitalAssetLinkCrossValidator() = default;

void DigitalAssetLinkCrossValidator::Start() {
  // Fetch and validate the manifest from the lookalike site.
  asset_link_handler_->CheckDigitalAssetLinkRelationship(
      lookalike_domain_.Serialize(), kDigitalAssetLinkRecordType, base::nullopt,
      {{"namespace", "web"}, {"site", target_domain_.Serialize()}},
      base::BindOnce(
          &DigitalAssetLinkCrossValidator::OnFetchLookalikeManifestComplete,
          base::Unretained(this)));
}

void DigitalAssetLinkCrossValidator::OnFetchLookalikeManifestComplete(
    digital_asset_links::RelationshipCheckResult result) {
  // Fail if the first manifest failed.
  if (result != digital_asset_links::RelationshipCheckResult::kSuccess) {
    std::move(callback_).Run(false);
    return;
  }
  // Swap current and target domains and validate the new manifest.
  asset_link_handler_->CheckDigitalAssetLinkRelationship(
      target_domain_.Serialize(), kDigitalAssetLinkRecordType, base::nullopt,
      {{"namespace", "web"}, {"site", lookalike_domain_.Serialize()}},
      base::BindOnce(
          &DigitalAssetLinkCrossValidator::OnFetchTargetManifestComplete,
          base::Unretained(this)));
}

void DigitalAssetLinkCrossValidator::OnFetchTargetManifestComplete(
    digital_asset_links::RelationshipCheckResult result) {
  std::move(callback_).Run(
      result == digital_asset_links::RelationshipCheckResult::kSuccess);
}
