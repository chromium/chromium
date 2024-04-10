// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_REFRESH_BANNER_VIEW_H_
#define ASH_SYSTEM_MAHI_REFRESH_BANNER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace views {
class Label;
}

namespace ash {

class MahiUiUpdate;
enum class VisibilityState;

class ASH_EXPORT RefreshBannerView : public views::FlexLayoutView,
                                     public MahiUiController::Delegate {
  METADATA_HEADER(RefreshBannerView, views::FlexLayoutView)

 public:
  explicit RefreshBannerView(MahiUiController* ui_controller);
  RefreshBannerView(const RefreshBannerView&) = delete;
  RefreshBannerView& operator=(const RefreshBannerView&) = delete;
  ~RefreshBannerView() override;

  // Shows/hides the refresh banner on top of the Mahi panel with animation.
  void Show();
  void Hide();

 private:
  // views::FlexLayoutView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // MahiUiController::Delegate:
  views::View* GetView() override;
  bool GetViewVisibility(VisibilityState state) const override;
  void OnUpdated(const MahiUiUpdate& update) override;

  // `ui_controller_` will outlive `this`.
  const raw_ptr<MahiUiController> ui_controller_;

  // Owned by the views hierarchy.
  raw_ptr<views::Label> title_label_ = nullptr;

  base::WeakPtrFactory<RefreshBannerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_REFRESH_BANNER_VIEW_H_
