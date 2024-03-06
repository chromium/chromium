// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_REFRESH_BANNER_VIEW_H_
#define ASH_SYSTEM_MAHI_REFRESH_BANNER_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

class ASH_EXPORT RefreshBannerView : public views::FlexLayoutView {
  METADATA_HEADER(RefreshBannerView, views::FlexLayoutView)

 public:
  RefreshBannerView();
  RefreshBannerView(const RefreshBannerView&) = delete;
  RefreshBannerView& operator=(const RefreshBannerView&) = delete;
  ~RefreshBannerView() override;

  // Shows the refresh banner on top of the Mahi panel by animating it.
  void Show();

  // Hides the refresh banner by animating it out.
  void Hide();

 private:
  // views::FlexLayoutView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  base::WeakPtrFactory<RefreshBannerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_REFRESH_BANNER_VIEW_H_
