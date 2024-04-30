// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTENTS_VIEW_H_
#define ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTENTS_VIEW_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {

class Label;
class ImageButton;

}  // namespace views

namespace ash {

class AnimatedRoundedImageView;
class AuthHubConnector;
class AuthPanel;
class NonAccessibleView;

// The parent view for in-session auth dialogs. This gets created,
// injected into a widget and shown as part of
// `InSessionAuthDialogController::ShowAuthDialog`.
// Hosts AuthPanel, as well as all of the elements around it, such as:
// User avatar, Title, Prompt, etc.
class InSessionAuthDialogContentsView : public views::View {
  METADATA_HEADER(InSessionAuthDialogContentsView, views::View)

 public:
  InSessionAuthDialogContentsView(const std::optional<std::string>& prompt,
                                  base::OnceClosure on_end_authentication,
                                  base::RepeatingClosure on_ui_initialized,
                                  AuthHubConnector* connector);
  ~InSessionAuthDialogContentsView() override;
  InSessionAuthDialogContentsView(const InSessionAuthDialogContentsView&) =
      delete;
  InSessionAuthDialogContentsView& operator=(
      const InSessionAuthDialogContentsView&) = delete;

  AuthPanel* GetAuthPanel();

 private:
  void AddVerticalSpacing(int height);
  void AddCloseButton();
  void AddUserAvatar();
  void AddTitle();
  void AddPrompt(const std::string& prompt);
  void AddAuthPanel(base::OnceClosure on_end_authentication,
                    base::RepeatingClosure on_ui_initialized,
                    AuthHubConnector* connector);

  raw_ptr<AnimatedRoundedImageView> avatar_view_ = nullptr;

  raw_ptr<views::Label> title_ = nullptr;

  raw_ptr<AuthPanel> auth_panel_;

  raw_ptr<views::Label> prompt_view_;

  raw_ptr<NonAccessibleView> close_button_container_;

  raw_ptr<views::ImageButton> close_button_;
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTENTS_VIEW_H_
