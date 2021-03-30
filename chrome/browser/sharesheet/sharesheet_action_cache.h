// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_ACTION_CACHE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_ACTION_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace gfx {
struct VectorIcon;
}

namespace sharesheet {

class ShareAction;

// The SharesheetActionCache facilitates communication between ShareActions
// and the SharesheetService.
class SharesheetActionCache {
 public:
  explicit SharesheetActionCache(Profile* profile);
  ~SharesheetActionCache();

  SharesheetActionCache(const SharesheetActionCache&) = delete;
  SharesheetActionCache& operator=(const SharesheetActionCache&) = delete;

  ShareAction* GetActionFromName(const std::u16string& action_name);

  const std::vector<std::unique_ptr<ShareAction>>& GetShareActions();

  bool HasVisibleActions(const apps::mojom::IntentPtr& intent,
                         bool contains_google_document);

  // Returns null if |display_name| is not a valid ShareAction.
  const gfx::VectorIcon* GetVectorIconFromName(
      const std::u16string& display_name);

 private:
  void AddShareAction(std::unique_ptr<ShareAction> action);

  std::vector<std::unique_ptr<ShareAction>> share_actions_;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_ACTION_CACHE_H_
