// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UI_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UI_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class OSFeedbackUI : public ui::MojoWebUIController {
 public:
  explicit OSFeedbackUI(content::WebUI* web_ui);
  OSFeedbackUI(const OSFeedbackUI&) = delete;
  OSFeedbackUI& operator=(const OSFeedbackUI&) = delete;
  ~OSFeedbackUI() override;
};

}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_OS_FEEDBACK_UI_H_
