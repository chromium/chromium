// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTENTS_VIEW_H_
#define ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTENTS_VIEW_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
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
  class TestApi {
   public:
    explicit TestApi(InSessionAuthDialogContentsView*);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    views::Button* GetCloseButton();

   private:
    raw_ptr<InSessionAuthDialogContentsView> contents_view_;
  };

  InSessionAuthDialogContentsView(const std::optional<std::string>& prompt,
                                  base::OnceClosure on_end_authentication,
                                  base::RepeatingClosure on_ui_initialized,
                                  AuthHubConnector* connector,
                                  AuthHub* auth_hub);
  ~InSessionAuthDialogContentsView() override;
  InSessionAuthDialogContentsView(const InSessionAuthDialogContentsView&) =
      delete;
  InSessionAuthDialogContentsView& operator=(
      const InSessionAuthDialogContentsView&) = delete;

  AuthPanel* GetAuthPanel();

  void ShowAuthError(AshAuthFactor factor);

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

 private:
  friend class TestApi;

  void AddVerticalSpacing(int height);
  void AddCloseButton();
  void AddUserAvatar();
  void AddTitle();
  void AddPrompt(const std::string& prompt);
  void AddAuthPanel(base::OnceClosure on_end_authentication,
                    base::RepeatingClosure on_ui_initialized,
                    AuthHubConnector* connector);

  void OnCloseButtonPressed();

  raw_ptr<AnimatedRoundedImageView> avatar_view_ = nullptr;

  raw_ptr<views::Label> title_ = nullptr;

  raw_ptr<AuthPanel> auth_panel_;

  raw_ptr<views::Label> prompt_view_;

  raw_ptr<NonAccessibleView> close_button_container_;

  raw_ptr<views::ImageButton> close_button_;

  raw_ptr<AuthHubConnector> connector_;

  raw_ptr<AuthHub> auth_hub_;

  base::WeakPtrFactory<InSessionAuthDialogContentsView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_CONTENTS_VIEW_H_
