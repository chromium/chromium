// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/metadata/updater_service.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace tpcd::metadata {
UpdaterService::UpdaterService(content::BrowserContext* context)
    : browser_context_(context) {
  CHECK(browser_context_);

  cookie_settings_ = CookieSettingsFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context_));

  parser_ = tpcd::metadata::Parser::GetInstance();
  if (!parser_->GetMetadata().empty()) {
    OnMetadataReady();
  }

  parser_->AddObserver(this);
}

UpdaterService::~UpdaterService() {
  parser_->RemoveObserver(this);
}

void UpdaterService::Shutdown() {
  parser_->RemoveObserver(this);
}

void UpdaterService::OnMetadataReady() {
  CHECK(browser_context_);

  ContentSettingsForOneType tpcd_metadata_grants;

  for (const auto& metadata_entry : parser_->GetMetadata()) {
    const auto primary_pattern = ContentSettingsPattern::FromString(
        metadata_entry.primary_pattern_spec());
    const auto secondary_pattern = ContentSettingsPattern::FromString(
        metadata_entry.secondary_pattern_spec());

    // This is unlikely to occurred as it is validated before the component is
    // installed by the component installer.
    if (!primary_pattern.IsValid() || !secondary_pattern.IsValid()) {
      continue;
    }

    base::Value value(ContentSetting::CONTENT_SETTING_ALLOW);

    tpcd_metadata_grants.emplace_back(primary_pattern, secondary_pattern,
                                      std::move(value), std::string(), false);
  }

  cookie_settings_->SetContentSettingsFor3pcdMetadataGrants(
      tpcd_metadata_grants);

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::TPCD_METADATA_GRANTS,
                           std::move(tpcd_metadata_grants),
                           base::NullCallback());
}
}  // namespace tpcd::metadata
