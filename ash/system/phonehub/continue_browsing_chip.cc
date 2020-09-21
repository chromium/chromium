// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/continue_browsing_chip.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kContinueBrowsingChipPadding(8, 8);
constexpr int kContinueBrowsingChipSpacing = 5;
constexpr int kContinueBrowsingChipFaviconSpacing = 5;
constexpr gfx::Size kContinueBrowsingChipFaviconSize(50, 50);
constexpr int kTaskContinuationChipRadius = 10;
constexpr int kTitleMaxLines = 2;
constexpr gfx::Size kTitleViewSize(100, 40);

}  // namespace

ContinueBrowsingChip::ContinueBrowsingChip(
    const chromeos::phonehub::BrowserTabsModel::BrowserTabMetadata& metadata)
    : views::Button(this), url_(metadata.url) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContinueBrowsingChipPadding,
      kContinueBrowsingChipSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  auto* header_view = AddChildView(std::make_unique<views::View>());
  auto* header_layout =
      header_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kContinueBrowsingChipFaviconSpacing));
  header_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  header_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  auto* favicon =
      header_view->AddChildView(std::make_unique<views::ImageView>());
  favicon->SetImageSize(kContinueBrowsingChipFaviconSize);
  favicon->SetImage(metadata.favicon.AsImageSkia());

  auto* title_label =
      header_view->AddChildView(std::make_unique<views::Label>(metadata.title));
  title_label->SetAutoColorReadabilityEnabled(false);
  title_label->SetSubpixelRenderingEnabled(false);
  title_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetMaxLines(kTitleMaxLines);
  title_label->SetSize(kTitleViewSize);
  title_label->SetFontList(
      title_label->font_list().DeriveWithWeight(gfx::Font::Weight::BOLD));

  auto* url_label = AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(metadata.url.host())));
  url_label->SetAutoColorReadabilityEnabled(false);
  url_label->SetSubpixelRenderingEnabled(false);
  url_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
}

void ContinueBrowsingChip::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  gfx::Rect bounds = GetContentsBounds();
  canvas->DrawRoundRect(bounds, kTaskContinuationChipRadius, flags);
  views::View::OnPaintBackground(canvas);
}

void ContinueBrowsingChip::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  PA_LOG(INFO) << "Opening browser tab: " << url_;
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      url_, /*from_user_interaction=*/true);
  Shell::GetPrimaryRootWindowController()
      ->GetStatusAreaWidget()
      ->phone_hub_tray()
      ->CloseBubble();
}

ContinueBrowsingChip::~ContinueBrowsingChip() = default;

const char* ContinueBrowsingChip::GetClassName() const {
  return "ContinueBrowsingChip";
}

}  // namespace ash
