// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_USER_CHOOSER_VIEW_H_
#define ASH_SYSTEM_UNIFIED_USER_CHOOSER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/media/media_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class UserChooserDetailedViewController;

// Circular image view with user's icon of |user_index|.
views::View* CreateUserAvatarView(int user_index);

// Get accessibility string for |user_index|.
std::u16string GetUserItemAccessibleString(int user_index);

// A button item of a switchable user.
class UserItemButton : public views::Button {
 public:
  METADATA_HEADER(UserItemButton);
  UserItemButton(PressedCallback callback,
                 UserChooserDetailedViewController* controller,
                 int user_index,
                 ax::mojom::Role role,
                 bool has_close_button);

  UserItemButton(const UserItemButton&) = delete;
  UserItemButton& operator=(const UserItemButton&) = delete;

  ~UserItemButton() override = default;

  void SetCaptureState(MediaCaptureState capture_states);

  // views::Button:
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  const int user_index_;
  const raw_ptr<views::ImageView, ExperimentalAsh> capture_icon_;
  const raw_ptr<views::Label, ExperimentalAsh> name_;
  const raw_ptr<views::Label, ExperimentalAsh> email_;
};

// A detailed view of user chooser.
class ASH_EXPORT UserChooserView : public views::View,
                                   public MediaCaptureObserver {
 public:
  METADATA_HEADER(UserChooserView);
  explicit UserChooserView(UserChooserDetailedViewController* controller);

  UserChooserView(const UserChooserView&) = delete;
  UserChooserView& operator=(const UserChooserView&) = delete;

  ~UserChooserView() override;

  // MediaCaptureObserver:
  void OnMediaCaptureChanged(const base::flat_map<AccountId, MediaCaptureState>&
                                 capture_states) override;

 private:
  std::vector<UserItemButton*> user_item_buttons_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_USER_CHOOSER_VIEW_H_
