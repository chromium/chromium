// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_HEADER_H_
#define ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_HEADER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class ChannelIndicatorQuickSettingsView;
class EolNoticeQuickSettingsView;
class ExtendedUpdatesNoticeQuickSettingsView;
class UnifiedSystemTrayController;

// The header view shown at the top of the `QuickSettingsView`. Contains an
// optional "Managed by" button and an optional release channel indicator. Sets
// itself invisible when its child views do not need to be shown. When both
// buttons are shown uses a two-column side-by-side layout.
class ASH_EXPORT QuickSettingsHeader : public views::View {
  METADATA_HEADER(QuickSettingsHeader, views::View)

 public:
  explicit QuickSettingsHeader(UnifiedSystemTrayController* controller);
  QuickSettingsHeader(const QuickSettingsHeader&) = delete;
  QuickSettingsHeader& operator=(const QuickSettingsHeader&) = delete;
  ~QuickSettingsHeader() override;

  // views::View:
  void ChildVisibilityChanged(views::View* child) override;

  ChannelIndicatorQuickSettingsView* channel_view_for_test() {
    return channel_view_;
  }

  EolNoticeQuickSettingsView* eol_notice_for_test() { return eol_notice_; }

  // Shows enterprise managed device information.
  static void ShowEnterpriseInfo(UnifiedSystemTrayController* controller,
                                 bool showManagementDisclosureDialog);

  views::View* GetManagedButtonForTest();
  views::View* GetSupervisedButtonForTest();
  views::Label* GetManagedButtonLabelForTest();
  views::Label* GetSupervisedButtonLabelForTest();
  views::View* GetExtendedUpdatesViewForTest();

 private:
  // A view that shows whether the device is enterprise managed or not. It
  // updates by observing `EnterpriseDomainModel`.
  class EnterpriseManagedView;

  // A base class of the views showing device management state.
  class ManagedStateView;

  // Updates visibility for this view. When it has no children it sets itself
  // invisible so it does not consume any space. Also updates the size of the
  // child views based on whether one or two columns are visible.
  void UpdateVisibilityAndLayout();

  // Owned by views hierarchy.
  raw_ptr<EnterpriseManagedView> enterprise_managed_view_ = nullptr;
  raw_ptr<ManagedStateView> supervised_view_ = nullptr;
  raw_ptr<ChannelIndicatorQuickSettingsView> channel_view_ = nullptr;
  raw_ptr<EolNoticeQuickSettingsView> eol_notice_ = nullptr;
  raw_ptr<ExtendedUpdatesNoticeQuickSettingsView> extended_updates_notice_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_QUICK_SETTINGS_HEADER_H_
