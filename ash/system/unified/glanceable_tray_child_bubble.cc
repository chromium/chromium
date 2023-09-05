// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_child_bubble.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_error_message_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {
constexpr int kBubbleCornerRadius = 24;

}  // namespace

GlanceableTrayChildBubble::GlanceableTrayChildBubble(
    DetailedViewDelegate* delegate,
    bool for_glanceables_container)
    : TrayDetailedView(delegate) {
  if (for_glanceables_container) {
    SetAccessibleRole(ax::mojom::Role::kGroup);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetIsFastRoundedCorner(true);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF{static_cast<float>(kBubbleCornerRadius)});
    // TODO(b:286941809): Setting blur here, can break the rounded corners
    // applied to the parent scroll view.
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

    SetBackground(views::CreateThemedSolidBackground(
        static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemBaseElevated)));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kBubbleCornerRadius,
        chromeos::features::IsJellyrollEnabled()
            ? views::HighlightBorder::Type::kHighlightBorderOnShadow
            : views::HighlightBorder::Type::kHighlightBorder1));
  }
}

void GlanceableTrayChildBubble::Layout() {
  views::View::Layout();
  if (error_message_) {
    error_message_->UpdateBoundsToContainer(GetLocalBounds());
  }
}

void GlanceableTrayChildBubble::ShowErrorMessage(
    const std::u16string& error_message) {
  MaybeDismissErrorMessage();

  error_message_ = AddChildView(std::make_unique<GlanceablesErrorMessageView>(
      base::BindRepeating(&GlanceableTrayChildBubble::MaybeDismissErrorMessage,
                          base::Unretained(this)),
      error_message));
  error_message_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  error_message_->SetID(
      base::to_underlying(GlanceablesViewId::kGlanceablesErrorMessageView));
}

void GlanceableTrayChildBubble::MaybeDismissErrorMessage() {
  if (!error_message_.get()) {
    return;
  }

  RemoveChildViewT(std::exchange(error_message_, nullptr));
}

BEGIN_METADATA(GlanceableTrayChildBubble, views::View)
END_METADATA

}  // namespace ash
