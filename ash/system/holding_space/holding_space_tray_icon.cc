// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_tray_icon_preview.h"
#include "ash/system/tray/tray_constants.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/stl_util.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns whether previews are enabled.
bool IsPreviewsEnabled() {
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  return features::IsTemporaryHoldingSpacePreviewsEnabled() && prefs &&
         holding_space_prefs::IsPreviewsEnabled(prefs);
}

// Sets `view`'s visibility to the specified value. Note that this method will
// only update visibility if necessary. This is to avoid propagating an event
// up the view tree when visibility is not changed that could otherwise result
// in layout invalidation.
void SetVisibility(views::View* view, bool visible) {
  if (view->GetVisible() != visible)
    view->SetVisible(visible);
}

}  // namespace

// HoldingSpaceTrayIcon --------------------------------------------------------

HoldingSpaceTrayIcon::HoldingSpaceTrayIcon(Shelf* shelf) : shelf_(shelf) {
  SetID(kHoldingSpaceTrayIconId);
  InitLayout();

  if (features::IsTemporaryHoldingSpacePreviewsEnabled()) {
    controller_observer_.Add(HoldingSpaceController::Get());
    shell_observer_.Add(Shell::Get());
    session_observer_.Add(Shell::Get()->session_controller());

    // It's possible that this holding space tray icon was created after login,
    // such as would occur if the user connects an external display. In such
    // situations the holding space model will already have been attached.
    if (HoldingSpaceController::Get()->model())
      OnHoldingSpaceModelAttached(HoldingSpaceController::Get()->model());
  }
}

HoldingSpaceTrayIcon::~HoldingSpaceTrayIcon() = default;

void HoldingSpaceTrayIcon::OnLocaleChanged() {
  TooltipTextChanged();
}

base::string16 HoldingSpaceTrayIcon::GetTooltipText(
    const gfx::Point& point) const {
  return l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_TITLE);
}

int HoldingSpaceTrayIcon::GetHeightForWidth(int width) const {
  // The parent for this view (`TrayContainer`) uses a `BoxLayout` for its
  // `LayoutManager`. When the shelf orientation is vertical, the `BoxLayout`
  // will also have vertical orientation and will invoke `GetHeightForWidth()`
  // instead of `GetPreferredSize()` when determining preferred size.
  return GetPreferredSize().height();
}

void HoldingSpaceTrayIcon::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));

  // No previews image.
  // NOTE: Events are disallowed on the `no_previews_image_view_` subtree so
  // that tooltips will be retrieved from `this` instead.
  no_previews_image_view_ = AddChildView(std::make_unique<views::ImageView>());
  no_previews_image_view_->SetCanProcessEventsWithinSubtree(false);
  no_previews_image_view_->SetImage(gfx::CreateVectorIcon(
      kHoldingSpaceIcon, kHoldingSpaceTrayIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));

  if (features::IsTemporaryHoldingSpacePreviewsEnabled()) {
    // As holding space items are added to the model, child layers will be added
    // to `this` view's layer to represent them.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    return;
  }
}

void HoldingSpaceTrayIcon::OnHoldingSpaceModelAttached(
    HoldingSpaceModel* model) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  model_observer_.Add(model);
  for (const std::unique_ptr<HoldingSpaceItem>& item : model->items())
    OnHoldingSpaceItemAdded(item.get());
}

void HoldingSpaceTrayIcon::OnHoldingSpaceModelDetached(
    HoldingSpaceModel* model) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  model_observer_.Remove(model);
  for (const std::unique_ptr<HoldingSpaceItem>& item : model->items())
    OnHoldingSpaceItemRemoved(item.get());
}

void HoldingSpaceTrayIcon::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  if (!previews_enabled_)
    return;

  if (!item->IsFinalized())
    return;

  SetVisibility(no_previews_image_view_, false);

  for (std::unique_ptr<HoldingSpaceTrayIconPreview>& preview : previews_)
    preview->AnimateShift();

  auto preview = std::make_unique<HoldingSpaceTrayIconPreview>(this, item);
  previews_.push_back(std::move(preview));
  previews_.back()->AnimateIn(/*index=*/0u);

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  if (!previews_enabled_)
    return;

  if (!item->IsFinalized())
    return;

  size_t index = previews_.size();
  for (size_t i = 0u; i < previews_.size(); ++i) {
    if (previews_[i]->item() == item) {
      index = i;
      break;
    }
  }

  DCHECK_LT(index, previews_.size());

  removed_previews_.push_back(std::move(previews_[index]));
  previews_.erase(previews_.begin() + index);

  for (int i = index - 1; i >= 0; --i)
    previews_[i]->AnimateUnshift();

  removed_previews_.back()->AnimateOut(base::BindOnce(
      &HoldingSpaceTrayIcon::OnHoldingSpaceTrayIconPreviewAnimatedOut,
      base::Unretained(this),
      base::Unretained(removed_previews_.back().get())));

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnHoldingSpaceItemFinalized(
    const HoldingSpaceItem* item) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  if (!previews_enabled_)
    return;

  if (previews_.empty()) {
    OnHoldingSpaceItemAdded(item);
    return;
  }

  SetVisibility(no_previews_image_view_, false);

  size_t index = 0;
  for (const auto& candidate :
       HoldingSpaceController::Get()->model()->items()) {
    if (candidate->id() == item->id())
      break;
    if (candidate->IsFinalized())
      ++index;
  }

  for (size_t i = 0; i < index; ++i)
    previews_[i]->AnimateShift();

  auto preview = std::make_unique<HoldingSpaceTrayIconPreview>(this, item);
  previews_.insert(previews_.begin() + index, std::move(preview));

  // NOTE: UI sort is inverse of model sort so take `index` from end.
  previews_[index]->AnimateIn(previews_.size() - index - 1);

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  if (!previews_enabled_)
    return;

  removed_previews_.clear();

  for (const auto& preview : previews_)
    preview->OnShelfAlignmentChanged(old_alignment, shelf_->alignment());

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  // NOTE: The callback being bound is scoped to `pref_change_registrar_` which
  // is owned by `this` so it is safe to bind with an unretained raw pointer.
  holding_space_prefs::AddPreviewsEnabledChangedCallback(
      pref_change_registrar_.get(),
      base::BindRepeating(&HoldingSpaceTrayIcon::UpdatePreviewsEnabled,
                          base::Unretained(this)));

  UpdatePreviewsEnabled();
}

void HoldingSpaceTrayIcon::UpdatePreferredSize() {
  if (!previews_enabled_) {
    SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize));
    return;
  }

  const int num_visible_previews =
      std::min(kHoldingSpaceTrayIconMaxVisiblePreviews,
               static_cast<int>(previews_.size()));

  int primary_axis_size = kTrayItemSize;
  if (num_visible_previews > 1)
    primary_axis_size += (num_visible_previews - 1) * kTrayItemSize / 2;

  gfx::Size preferred_size = shelf_->PrimaryAxisValue(
      /*horizontal=*/gfx::Size(primary_axis_size, kTrayItemSize),
      /*vertical=*/gfx::Size(kTrayItemSize, primary_axis_size));

  if (preferred_size != GetPreferredSize())
    SetPreferredSize(preferred_size);
}

void HoldingSpaceTrayIcon::UpdatePreviewsEnabled() {
  DCHECK(features::IsTemporaryHoldingSpacePreviewsEnabled());

  const bool previews_enabled = IsPreviewsEnabled();
  if (previews_enabled_ == previews_enabled)
    return;

  if (previews_enabled)
    ShowPreviews();
  else
    HidePreviews();
}

void HoldingSpaceTrayIcon::ShowPreviews() {
  if (previews_enabled_)
    return;

  previews_enabled_ = true;

  if (HoldingSpaceController::Get()->model()) {
    for (const auto& item : HoldingSpaceController::Get()->model()->items())
      OnHoldingSpaceItemAdded(item.get());
  }
}

void HoldingSpaceTrayIcon::HidePreviews() {
  if (!previews_enabled_)
    return;

  previews_enabled_ = false;

  previews_.clear();
  removed_previews_.clear();

  SetVisibility(no_previews_image_view_, true);

  UpdatePreferredSize();
}

void HoldingSpaceTrayIcon::OnHoldingSpaceTrayIconPreviewAnimatedOut(
    HoldingSpaceTrayIconPreview* preview) {
  base::EraseIf(removed_previews_, base::MatchesUniquePtr(preview));

  if (previews_.empty() && removed_previews_.empty())
    SetVisibility(no_previews_image_view_, true);
}

BEGIN_METADATA(HoldingSpaceTrayIcon, views::View)
END_METADATA

}  // namespace ash
