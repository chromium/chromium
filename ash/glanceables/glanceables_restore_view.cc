// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_restore_view.h"

#include <memory>

#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_util.h"
#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "base/bind.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

constexpr gfx::Size kScreenshotTargetSize(300, 200);

void OnButtonPressed() {
  Shell::Get()->glanceables_controller()->RestoreSession();
}

}  // namespace

GlanceablesRestoreView::GlanceablesRestoreView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);

  image_util::DecodeImageFile(
      base::BindOnce(&GlanceablesRestoreView::OnSignoutScreenshotDecoded,
                     weak_ptr_factory_.GetWeakPtr()),
      glanceables_util::GetSignoutScreenshotPath(),
      data_decoder::mojom::ImageCodec::kPng);
}

GlanceablesRestoreView::~GlanceablesRestoreView() = default;

void GlanceablesRestoreView::OnSignoutScreenshotDecoded(
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // There is no image from previous shutdown or sign-out.
    AddPillButton();
    return;
  }
  AddImageButton(image);
}

void GlanceablesRestoreView::AddImageButton(const gfx::ImageSkia& image) {
  image_button_ = AddChildView(std::make_unique<views::ImageButton>(
      base::BindRepeating(&OnButtonPressed)));
  image_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_GLANCEABLES_RESTORE_SESSION));
  image_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::ImageSkiaOperations::CreateResizedImage(
          image, skia::ImageOperations::ResizeMethod::RESIZE_BETTER,
          kScreenshotTargetSize));
}

void GlanceablesRestoreView::AddPillButton() {
  pill_button_ = AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&OnButtonPressed),
      l10n_util::GetStringUTF16(IDS_GLANCEABLES_RESTORE)));
  // TODO(crbug.com/1353119): Use color provider.
  pill_button_->SetButtonTextColor(gfx::kGoogleGrey800);
  pill_button_->SetBackgroundColor(gfx::kGoogleGrey500);
}

}  // namespace ash
