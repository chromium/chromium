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

namespace views {
class Button;
class View;
}  // namespace views

namespace ash {

class HoverHighlightView;
class NearbyShareDelegate;
class RoundedContainer;
class Switch;

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
  void HandleViewClicked(views::View* view) override;

  void CreateIsEnabledContainer();
  void CreateVisibilitySelectionContainer();
  void CreateYourDevicesRow();
  void CreateContactsRow();
  void CreateHiddenRow();
  void CreateVisibilityRow(HoverHighlightView* visibility_row,
                           const gfx::VectorIcon& vector_icon,
                           const std::u16string& label,
                           const std::u16string& sublabel);
  void CreateEveryoneRow();

  void FormatEveryoneRow(const ui::ColorId color_id,
                         const bool in_high_visibility,
                         const bool is_row_enabled);
  void FormatVisibilitySelectionContainer(const bool in_high_visibility);

  void OnSettingsButtonClicked();
  void OnQuickShareToggleClicked();
  void OnYourDevicesSelected();
  void OnContactsSelected();
  void OnHiddenSelected();
  void OnEveryoneToggleClicked();

  void SetCheckCircle(const bool in_high_visibility);

  const std::u16string user_email_;

  raw_ptr<views::Button> settings_button_ = nullptr;
  raw_ptr<RoundedContainer> is_enabled_container_ = nullptr;
  raw_ptr<HoverHighlightView> toggle_row_ = nullptr;
  // TODO(brandosocarras, b/360150790): use `toggle_switch_` in this class, not
  // just in utest.
  raw_ptr<Switch> quick_share_toggle_ = nullptr;
  raw_ptr<RoundedContainer> visibility_selection_container_ = nullptr;
  raw_ptr<HoverHighlightView> your_devices_row_ = nullptr;
  raw_ptr<HoverHighlightView> contacts_row_ = nullptr;
  raw_ptr<HoverHighlightView> hidden_row_ = nullptr;
  raw_ptr<HoverHighlightView> everyone_row_ = nullptr;
  raw_ptr<Switch> everyone_toggle_ = nullptr;

  const raw_ptr<NearbyShareDelegate> nearby_share_delegate_;

  base::WeakPtrFactory<NearbyShareDetailedViewImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NEARBY_SHARE_NEARBY_SHARE_DETAILED_VIEW_IMPL_H_
