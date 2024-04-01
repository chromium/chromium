// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/share_action/share_action_cache.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/share_action/example_action.h"
#include "chrome/browser/sharesheet/share_action/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/sharesheet/copy_to_clipboard_share_action.h"
#include "chrome/browser/ash/sharesheet/drive_share_action.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"
#endif

namespace sharesheet {

ShareActionCache::ShareActionCache(Profile* profile) {
  // ShareActions will be initialised here by calling AddShareAction.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (NearbySharingServiceFactory::IsNearbyShareSupportedForBrowserContext(
          profile)) {
    AddShareAction(std::make_unique<NearbyShareAction>(profile));
  }
  AddShareAction(std::make_unique<ash::sharesheet::DriveShareAction>());
  AddShareAction(
      std::make_unique<ash::sharesheet::CopyToClipboardShareAction>(profile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ShareActionCache::~ShareActionCache() = default;

const std::vector<std::unique_ptr<ShareAction>>&
ShareActionCache::GetShareActions() {
  return share_actions_;
}

ShareAction* ShareActionCache::GetActionFromType(ShareActionType action_type) {
  auto iter = share_actions_.begin();
  while (iter != share_actions_.end()) {
    if ((*iter)->GetActionType() == action_type) {
      return iter->get();
    } else {
      iter++;
    }
  }
  return nullptr;
}

const gfx::VectorIcon* ShareActionCache::GetVectorIconFromType(
    ShareActionType action_type) {
  ShareAction* share_action = GetActionFromType(action_type);
  if (share_action == nullptr) {
    return nullptr;
  }
  return &share_action->GetActionIcon();
}

bool ShareActionCache::HasVisibleActions(const apps::IntentPtr& intent,
                                         bool contains_google_document) {
  for (auto& action : share_actions_) {
    if (action->ShouldShowAction(intent, contains_google_document)) {
      return true;
    }
  }
  return false;
}

void ShareActionCache::AddShareActionForTesting() {
  AddShareAction(std::make_unique<ExampleAction>());
}

void ShareActionCache::AddShareAction(std::unique_ptr<ShareAction> action) {
  share_actions_.push_back(std::move(action));
}

}  // namespace sharesheet
