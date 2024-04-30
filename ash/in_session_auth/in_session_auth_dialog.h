// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_H_
#define ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_H_

#include <cstdint>
#include <memory>
#include <string>

#include "ash/in_session_auth/auth_dialog_contents_view.h"
#include "ash/public/cpp/session/user_info.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

// InSessionAuthDialog gets instantiated on every request to show
// an authentication dialog, and gets destroyed when the request is
// completed.
class InSessionAuthDialog {
 public:
  InSessionAuthDialog(
      uint32_t auth_methods,
      aura::Window* parent_window,
      const std::string& origin_name,
      const AuthDialogContentsView::AuthMethodsMetadata& auth_metadata,
      const UserAvatar& avatar);
  InSessionAuthDialog(const InSessionAuthDialog&) = delete;
  InSessionAuthDialog& operator=(const InSessionAuthDialog&) = delete;
  ~InSessionAuthDialog();

  views::Widget* widget() { return widget_.get(); }

  bool is_shown() const { return !!widget_; }

  uint32_t GetAuthMethods() const;

 private:
  // The dialog widget. Owned by this class so that we can close the widget
  // when auth completes.
  std::unique_ptr<views::Widget> widget_;

  // Pointer to the contents view. Used to query and update the set of available
  // auth methods.
  const uint32_t auth_methods_;
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_IN_SESSION_AUTH_DIALOG_H_
