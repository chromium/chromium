// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharesheet/share_action/share_action.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "components/services/app_service/public/cpp/intent.h"

class Profile;

// NearbyShareAction is responsible for loading the Nearby UI within the
// Sharesheet. A single instance is created on sign in to handle all operations.
class NearbyShareAction : public sharesheet::ShareAction {
 public:
  explicit NearbyShareAction(Profile* profile);
  ~NearbyShareAction() override;
  NearbyShareAction(const NearbyShareAction&) = delete;
  NearbyShareAction& operator=(const NearbyShareAction&) = delete;

  // sharesheet::ShareAction:
  sharesheet::ShareActionType GetActionType() const override;
  const std::u16string GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::IntentPtr intent) override;
  void OnClosing(sharesheet::SharesheetController* controller) override {}
  bool HasActionView() override;
  bool ShouldShowAction(const apps::IntentPtr& intent,
                        bool contains_hosted_document) override;
  bool OnAcceleratorPressed(const ui::Accelerator& accelerator) override;
  void SetActionCleanupCallbackForArc(
      base::OnceCallback<void()> callback) override;

  static std::vector<std::unique_ptr<Attachment>> CreateAttachmentsFromIntent(
      Profile* profile,
      apps::IntentPtr intent);

  void SetNearbyShareDisabledByPolicyForTesting(bool disabled) {
    nearby_share_disabled_by_policy_for_testing_ = disabled;
  }

 private:
  bool IsNearbyShareDisabledByPolicy();

  raw_ptr<Profile> profile_;
  std::optional<bool> nearby_share_disabled_by_policy_for_testing_ =
      std::nullopt;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_
