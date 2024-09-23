// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UPDATE_QUICK_SETTINGS_NOTICE_VIEW_PIXELTEST_BASE_H_
#define ASH_SYSTEM_UPDATE_QUICK_SETTINGS_NOTICE_VIEW_PIXELTEST_BASE_H_

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Pixel test base class for derived classes of the QuickSettingsNoticeView.
class QuickSettingsNoticeViewPixelTestBase : public AshTestBase {
 public:
  QuickSettingsNoticeViewPixelTestBase();
  QuickSettingsNoticeViewPixelTestBase(
      const QuickSettingsNoticeViewPixelTestBase&) = delete;
  QuickSettingsNoticeViewPixelTestBase& operator=(
      const QuickSettingsNoticeViewPixelTestBase&) = delete;
  ~QuickSettingsNoticeViewPixelTestBase() override;

  void SetUp() override;
  void TearDown() override;

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override;

 protected:
  // Adds the derived child view to the view hierarchy.
  template <class T>
  T* AddChildView(std::unique_ptr<T> child_view) {
    auto* view =
        widget_->GetContentsView()->AddChildView(std::move(child_view));
    // Use the default size from go/cros-quick-settings-spec
    view->SetPreferredSize(gfx::Size(408, 32));
    return view;
  }

  // Perform pixel diffing.
  void DiffView(size_t revision_number);

  // Container widget.
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UPDATE_QUICK_SETTINGS_NOTICE_VIEW_PIXELTEST_BASE_H_
