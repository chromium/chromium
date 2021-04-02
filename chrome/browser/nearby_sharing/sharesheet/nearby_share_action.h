// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_

#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"

namespace views {
class WebView;
}  // namespace views

class NearbyShareAction : public sharesheet::ShareAction,
                          content::WebContentsDelegate {
 public:
  NearbyShareAction();
  ~NearbyShareAction() override;
  NearbyShareAction(const NearbyShareAction&) = delete;
  NearbyShareAction& operator=(const NearbyShareAction&) = delete;

  // sharesheet::ShareAction:
  const std::u16string GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::mojom::IntentPtr intent) override;
  void OnClosing(sharesheet::SharesheetController* controller) override {}
  bool ShouldShowAction(const apps::mojom::IntentPtr& intent,
                        bool contains_hosted_document) override;
  bool OnAcceleratorPressed(const ui::Accelerator& accelerator) override;

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;

  static std::vector<std::unique_ptr<Attachment>> CreateAttachmentsFromIntent(
      Profile* profile,
      apps::mojom::IntentPtr intent);

  void SetNearbyShareDisabledByPolicyForTesting(bool disabled) {
    nearby_share_disabled_by_policy_for_testing_ = disabled;
  }

 private:
  bool IsNearbyShareDisabledByPolicy();

  base::Optional<bool> nearby_share_disabled_by_policy_for_testing_ =
      base::nullopt;
  views::WebView* web_view_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_
