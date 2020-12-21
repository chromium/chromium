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
                          nearby_share::NearbyShareDialogUI::Observer,
                          content::WebContentsDelegate {
 public:
  NearbyShareAction();
  ~NearbyShareAction() override;
  NearbyShareAction(const NearbyShareAction&) = delete;
  NearbyShareAction& operator=(const NearbyShareAction&) = delete;

  // sharesheet::ShareAction:
  const base::string16 GetActionName() override;
  const gfx::VectorIcon& GetActionIcon() override;
  void LaunchAction(sharesheet::SharesheetController* controller,
                    views::View* root_view,
                    apps::mojom::IntentPtr intent) override;
  void OnClosing(sharesheet::SharesheetController* controller) override;
  bool ShouldShowAction(const apps::mojom::IntentPtr& intent,
                        bool contains_hosted_document) override;

  // nearby_share::NearbyShareDialogUI::Observer:
  void OnClose() override;

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

 private:
  sharesheet::SharesheetController* controller_ = nullptr;
  nearby_share::NearbyShareDialogUI* nearby_ui_ = nullptr;
  views::WebView* web_view_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_ACTION_H_
