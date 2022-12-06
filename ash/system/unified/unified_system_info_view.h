// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_INFO_VIEW_H_
#define ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_INFO_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/gtest_prod_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class ManagementPowerDateComboView;
class ChannelIndicatorQuickSettingsView;

// A view at the bottom of UnifiedSystemTray bubble that shows system
// information.
class ASH_EXPORT UnifiedSystemInfoView : public views::View {
 public:
  METADATA_HEADER(UnifiedSystemInfoView);
  explicit UnifiedSystemInfoView(UnifiedSystemTrayController* controller);

  UnifiedSystemInfoView(const UnifiedSystemInfoView&) = delete;
  UnifiedSystemInfoView& operator=(const UnifiedSystemInfoView&) = delete;

  ~UnifiedSystemInfoView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // Introspection methods needed for unit tests.
  bool IsSupervisedVisibleForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(UnifiedSystemInfoViewTest, EnterpriseManagedVisible);
  FRIEND_TEST_ALL_PREFIXES(UnifiedSystemInfoViewTest,
                           EnterpriseManagedVisibleForActiveDirectory);
  FRIEND_TEST_ALL_PREFIXES(UnifiedSystemInfoViewTest,
                           EnterpriseUserManagedVisible);
  FRIEND_TEST_ALL_PREFIXES(UnifiedSystemInfoViewTest,
                           UpdateFiresAccessibilityEvents);
  FRIEND_TEST_ALL_PREFIXES(UnifiedSystemInfoViewNoSessionTest, ChildVisible);

  // Raw pointer to the combo view (owned by `UnifiedSystemInfoView`) that
  // facilitates introspection needed for unit tests.
  ManagementPowerDateComboView* combo_view_ = nullptr;

  // Raw pointer to the channel indicator quick settings view (owned by
  // `UnifiedSystemInfoView`) that facilitates introspection needed for unit
  // tests.
  ChannelIndicatorQuickSettingsView* channel_view_ = nullptr;

  // Introspection methods needed for unit tests.
  views::View* GetDateViewForTesting();
  views::View* GetDateViewLabelForTesting();
  void UpdateDateViewForTesting();
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_UNIFIED_SYSTEM_INFO_VIEW_H_
