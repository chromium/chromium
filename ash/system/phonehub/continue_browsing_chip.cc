// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/continue_browsing_chip.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance in dip.
constexpr gfx::Insets kContinueBrowsingChipInsets(8);
constexpr int kContinueBrowsingChipSpacing = 8;
constexpr int kContinueBrowsingChipFaviconSpacing = 8;
constexpr gfx::Size kContinueBrowsingChipFaviconSize(16, 16);
constexpr int kTaskContinuationChipRadius = 8;
constexpr int kTitleMaxLines = 2;

}  // namespace

ContinueBrowsingChip::ContinueBrowsingChip(
    const phonehub::BrowserTabsModel::BrowserTabMetadata& metadata,
    int index,
    size_t total_count,
    phonehub::UserActionRecorder* user_action_recorder)
    : views::Button(base::BindRepeating(&ContinueBrowsingChip::ButtonPressed,
                                        base::Unretained(this))),
      url_(metadata.url),
      index_(index),
      total_count_(total_count),
      user_action_recorder_(user_action_recorder) {
  auto* color_provider = AshColorProvider::Get();
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

  // Install this highlight path generator to set the desired shape for
  // our focus ring.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kTaskContinuationChipRadius);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContinueBrowsingChipInsets,
      kContinueBrowsingChipSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Inits the header view which consists of the favicon image and the url.
  auto* header_view = AddChildView(std::make_unique<views::View>());
  auto* header_layout =
      header_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kContinueBrowsingChipFaviconSpacing));
  header_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  header_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* favicon =
      header_view->AddChildView(std::make_unique<views::ImageView>());
  favicon->SetImageSize(kContinueBrowsingChipFaviconSize);

  if (metadata.favicon.IsEmpty()) {
    favicon->SetImage(CreateVectorIcon(
        kPhoneHubDefaultFaviconIcon,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
  } else {
    favicon->SetImage(metadata.favicon.AsImageSkia());
  }

  auto* url_label = header_view->AddChildView(
      std::make_unique<views::Label>(base::UTF8ToUTF16(metadata.url.host())));
  url_label->SetAutoColorReadabilityEnabled(false);
  url_label->SetSubpixelRenderingEnabled(false);
  url_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  url_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosAnnotation1,
                                        *url_label);

  auto* title_label =
      AddChildView(std::make_unique<views::Label>(metadata.title));
  title_label->SetAutoColorReadabilityEnabled(false);
  title_label->SetSubpixelRenderingEnabled(false);
  title_label->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetMaxLines(kTitleMaxLines);

  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosAnnotation2,
                                        *title_label);

  const std::u16string card_label = l10n_util::GetStringFUTF16(
      IDS_ASH_PHONE_HUB_CONTINUE_BROWSING_TAB_LABEL,
      base::NumberToString16(index_ + 1), base::NumberToString16(total_count_),
      metadata.title, base::UTF8ToUTF16(url_.spec()));
  SetTooltipText(card_label);
  GetViewAccessibility().SetName(card_label);
}

void ContinueBrowsingChip::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(
      GetColorProvider()->GetColor(kColorAshControlBackgroundColorInactive));
  gfx::Rect bounds = GetContentsBounds();
  canvas->DrawRoundRect(bounds, kTaskContinuationChipRadius, flags);
  views::View::OnPaintBackground(canvas);
}

ContinueBrowsingChip::~ContinueBrowsingChip() = default;

void ContinueBrowsingChip::ButtonPressed() {
  PA_LOG(INFO) << "Opening browser tab: " << url_;
  phone_hub_metrics::LogTabContinuationChipClicked(index_);
  user_action_recorder_->RecordBrowserTabOpened();

  NewWindowDelegate::GetPrimary()->OpenUrl(
      url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);

  // Close Phone Hub bubble in current display.
  views::Widget* const widget = GetWidget();
  // |widget| is null when this function is called before the view is added to a
  // widget (in unit tests).
  if (!widget)
    return;
  int64_t current_display_id =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget->GetNativeWindow())
          .id();
  Shell::GetRootWindowControllerWithDisplayId(current_display_id)
      ->GetStatusAreaWidget()
      ->phone_hub_tray()
      ->CloseBubble();
}

BEGIN_METADATA(ContinueBrowsingChip)
END_METADATA

}  // namespace ash
