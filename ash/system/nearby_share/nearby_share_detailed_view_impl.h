// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_IMPL_H_
#define ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/nearby_share/nearby_share_detailed_view.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace view {
class Button;
class View;
}  // namespace view

namespace ash {

class DetailedViewDelegate;

class ASH_EXPORT NearbyShareDetailedViewImpl : public NearbyShareDetailedView,
                                               public TrayDetailedView {
  METADATA_HEADER(NearbyShareDetailedViewImpl, TrayDetailedView)

 public:
  explicit NearbyShareDetailedViewImpl(
      DetailedViewDelegate* detailed_view_delegate);
  NearbyShareDetailedViewImpl(const NearbyShareDetailedViewImpl&) = delete;
  NearbyShareDetailedViewImpl& operator=(const NearbyShareDetailedViewImpl&) =
      delete;
  ~NearbyShareDetailedViewImpl() override;

  // NearbyShareDetailedView:
  views::View* GetAsView() override;

 private:
  friend class NearbyShareDetailedViewImplTest;

  // TrayDetailedView:
  void CreateExtraTitleRowButtons() override;

  void OnSettingsButtonClicked();

  raw_ptr<views::Button> settings_button_ = nullptr;

  base::WeakPtrFactory<NearbyShareDetailedViewImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_IMPL_H_
