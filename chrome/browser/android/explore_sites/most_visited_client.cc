// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/most_visited_client.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/explore_sites/explore_sites_feature.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace explore_sites {
using chrome::android::explore_sites::GetMostLikelyVariation;
using chrome::android::explore_sites::MostLikelyVariation;

std::unique_ptr<MostVisitedClient> MostVisitedClient::Create() {
  if (GetMostLikelyVariation() == MostLikelyVariation::NONE)
    return nullptr;

  // note: wrap_unique is used because the constructor is private.
  return base::WrapUnique(new MostVisitedClient());
}

MostVisitedClient::~MostVisitedClient() = default;

GURL MostVisitedClient::GetExploreSitesUrl() const {
  return GURL(chrome::kChromeUINativeExploreURL);
}

base::string16 MostVisitedClient::GetExploreSitesTitle() const {
  return l10n_util::GetStringUTF16(IDS_NTP_EXPLORE_SITES_TILE_TITLE);
}

MostVisitedClient::MostVisitedClient() = default;

}  // namespace explore_sites
