// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_VIEW_H_
#define ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/public_account_menu_view.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

class ManagedWarningView;

// Implements an expanded view for the public account user to select language
// and keyboard options.
class ASH_EXPORT ManagementDisclosureView : public NonAccessibleView {
  METADATA_HEADER(ManagementDisclosureView, NonAccessibleView)

 public:
  using OnManagementDisclosureDismissed = base::RepeatingClosure;
  explicit ManagementDisclosureView(
      const OnManagementDisclosureDismissed& on_dismissed);

  ManagementDisclosureView(const ManagementDisclosureView&) = delete;
  ManagementDisclosureView& operator=(const ManagementDisclosureView&) = delete;

  ~ManagementDisclosureView() override;

  void ProcessPressedEvent(const ui::LocatedEvent* event);
  void Hide();

  static gfx::Size GetPreferredSizeLandscape();
  static gfx::Size GetPreferredSizePortrait();

  // views::View:
  void Layout(PassKey) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  void UseLandscapeLayout();
  void UsePortraitLayout();

  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<ManagedWarningView> managed_warning_view_ = nullptr;
  raw_ptr<views::View> disclosure_view_ = nullptr;
  raw_ptr<PillButton> close_button_ = nullptr;
  raw_ptr<views::Label> admin_description_label_ = nullptr;
  raw_ptr<views::Label> additional_information_label_ = nullptr;
  raw_ptr<views::Label> may_be_able_to_view_title_ = nullptr;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  OnManagementDisclosureDismissed on_dismissed_;
  std::unique_ptr<ui::EventHandler> event_handler_;
  std::unique_ptr<SystemShadow> shadow_;

  base::WeakPtrFactory<ManagementDisclosureView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_MANAGEMENT_DISCLOSURE_VIEW_H_
