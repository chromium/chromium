// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UI_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"
#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class OSFeedbackUI;
class OsFeedbackDelegate;

// WebUIConfig for chrome://os-feedback
class OSFeedbackUIConfig : public ChromeOSWebUIConfig<OSFeedbackUI> {
 public:
  explicit OSFeedbackUIConfig(CreateWebUIControllerFunc create_controller_func)
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUIOSFeedbackHost,
                            create_controller_func) {}
};

class OSFeedbackUI : public ui::MojoWebDialogUI {
 public:
  OSFeedbackUI(content::WebUI* web_ui,
               std::unique_ptr<OsFeedbackDelegate> feedback_delegate);
  OSFeedbackUI(const OSFeedbackUI&) = delete;
  OSFeedbackUI& operator=(const OSFeedbackUI&) = delete;
  ~OSFeedbackUI() override;

  void BindInterface(
      mojo::PendingReceiver<os_feedback_ui::mojom::FeedbackServiceProvider>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<os_feedback_ui::mojom::HelpContentProvider>
          receiver);

 private:
  std::unique_ptr<feedback::HelpContentProvider> help_content_provider_;
  std::unique_ptr<feedback::FeedbackServiceProvider> feedback_service_provider_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UI_H_
