// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_action_cache.h"

#include "chrome/browser/browser_features.h"
#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"
#include "chrome/browser/sharesheet/drive_share_action.h"
#endif

namespace sharesheet {

SharesheetActionCache::SharesheetActionCache() {
  // ShareActions will be initialised here by calling AddShareAction.
#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kNearbySharing)) {
    AddShareAction(std::make_unique<NearbyShareAction>());
  }
  AddShareAction(std::make_unique<DriveShareAction>());
#endif
}

SharesheetActionCache::~SharesheetActionCache() = default;

const std::vector<std::unique_ptr<ShareAction>>&
SharesheetActionCache::GetShareActions() {
  return share_actions_;
}

ShareAction* SharesheetActionCache::GetActionFromName(
    const base::string16& action_name) {
  auto iter = share_actions_.begin();
  while (iter != share_actions_.end()) {
    if ((*iter)->GetActionName() == action_name) {
      return iter->get();
    } else {
      iter++;
    }
  }
  return nullptr;
}

bool SharesheetActionCache::HasVisibleActions(
    const apps::mojom::IntentPtr& intent,
    bool contains_google_document) {
  for (auto& action : share_actions_) {
    if (action->ShouldShowAction(intent, contains_google_document)) {
      return true;
    }
  }
  return false;
}

void SharesheetActionCache::AddShareAction(
    std::unique_ptr<ShareAction> action) {
  DCHECK_EQ(action->GetActionIcon().size(), gfx::Size(kIconSize, kIconSize));
  share_actions_.push_back(std::move(action));
}

}  // namespace sharesheet
