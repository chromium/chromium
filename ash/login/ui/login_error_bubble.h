// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
#define ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/style/ash_color_provider.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/view.h"

namespace ash {

// An error bubble shown on the login/lock screen, for example the bubble
// shown for wrong passwords. Always contains a warning sign at the top.
// The rest of the bubble is made up of a customizable view  supplied via
// `SetContent`.
class ASH_EXPORT LoginErrorBubble : public LoginBaseBubbleView {
 public:
  LoginErrorBubble();
  explicit LoginErrorBubble(base::WeakPtr<views::View> anchor_view);

  LoginErrorBubble(const LoginErrorBubble&) = delete;
  LoginErrorBubble& operator=(const LoginErrorBubble&) = delete;

  ~LoginErrorBubble() override;

  views::View* GetContent();
  // If the content is theme-change sensitive, it should be updated by the
  // class managing this instance via a new call to SetContent.
  void SetContent(std::unique_ptr<views::View> content);
  // Covers most cases where content is a simple label containing a message.
  // The eventual theme changes will be handled internally.
  void SetTextContent(const std::u16string& message);
  // We set an accessible name when content is not accessible. This happens if
  // content is a container (e.g. a text and a "learn more" button). In such a
  // case, it will have multiple subviews but only one which needs to be read
  // on bubble show – when the alert event occurs.
  void set_accessible_name(const std::u16string& name) {
    accessible_name_ = name;
  }

  // views::View:
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // LoginBaseBubbleView:
  void OnThemeChanged() override;

 private:
  views::View* content_ = nullptr;
  views::ImageView* alert_icon_ = nullptr;

  // Accessibility data.
  std::u16string accessible_name_;

  std::u16string message_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
