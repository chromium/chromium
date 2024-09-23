// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
#define ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/style/ash_color_provider.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// An error bubble shown on the login/lock screen, for example the bubble
// shown for wrong passwords. Always contains a warning sign at the top.
// The rest of the bubble is made up of a customizable view  supplied via
// `SetContent`.
class ASH_EXPORT LoginErrorBubble : public LoginBaseBubbleView {
  METADATA_HEADER(LoginErrorBubble, LoginBaseBubbleView)

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

 private:
  raw_ptr<views::View, DanglingUntriaged> content_ = nullptr;
  raw_ptr<views::ImageView> alert_icon_ = nullptr;

  std::u16string message_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_ERROR_BUBBLE_H_
