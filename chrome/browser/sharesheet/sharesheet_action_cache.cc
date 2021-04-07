// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_action_cache.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/example_action.h"
#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/common/chrome_features.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"
#include "chrome/browser/sharesheet/drive_share_action.h"
#endif

namespace sharesheet {

SharesheetActionCache::SharesheetActionCache(Profile* profile) {
  // ShareActions will be initialised here by calling AddShareAction.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          profile)) {
    AddShareAction(std::make_unique<NearbyShareAction>());
  }
  AddShareAction(std::make_unique<DriveShareAction>());
  // Add 9 example actions to show expanded view
  if (base::FeatureList::IsEnabled(features::kSharesheetContentPreviews)) {
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
    AddShareAction(std::make_unique<ExampleAction>());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

SharesheetActionCache::~SharesheetActionCache() = default;

const std::vector<std::unique_ptr<ShareAction>>&
SharesheetActionCache::GetShareActions() {
  return share_actions_;
}

ShareAction* SharesheetActionCache::GetActionFromName(
    const std::u16string& action_name) {
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

const gfx::VectorIcon* SharesheetActionCache::GetVectorIconFromName(
    const std::u16string& display_name) {
  ShareAction* share_action = GetActionFromName(display_name);
  if (share_action == nullptr) {
    return nullptr;
  }
  return &share_action->GetActionIcon();
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
  share_actions_.push_back(std::move(action));
}

}  // namespace sharesheet
