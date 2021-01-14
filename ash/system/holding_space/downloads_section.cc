// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/downloads_section.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// CallbackPathGenerator -------------------------------------------------------

class CallbackPathGenerator : public views::HighlightPathGenerator {
 public:
  using Callback = base::RepeatingCallback<gfx::RRectF()>;

  explicit CallbackPathGenerator(Callback callback) : callback_(callback) {}
  CallbackPathGenerator(const CallbackPathGenerator&) = delete;
  CallbackPathGenerator& operator=(const CallbackPathGenerator&) = delete;
  ~CallbackPathGenerator() override = default;

 private:
  // views::HighlightPathGenerator:
  base::Optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return callback_.Run();
  }

  Callback callback_;
};

// Header ----------------------------------------------------------------------

class Header : public views::Button {
 public:
  Header() {
    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE));
    SetCallback(
        base::BindRepeating(&Header::OnPressed, base::Unretained(this)));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
        kHoldingSpaceDownloadsHeaderSpacing));

    // Label.
    auto* label = AddChildView(holding_space_util::CreateLabel(
        holding_space_util::LabelStyle::kHeader,
        l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_DOWNLOADS_TITLE)));
    label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    layout->SetFlexForView(label, 1);

    // Chevron.
    AshColorProvider* const ash_color_provider = AshColorProvider::Get();
    auto* chevron = AddChildView(std::make_unique<views::ImageView>());
    chevron->SetFlipCanvasOnPaintForRTLUI(true);
    chevron->SetImage(gfx::CreateVectorIcon(
        kChevronRightIcon, kHoldingSpaceDownloadsChevronIconSize,
        ash_color_provider->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));

    // Focus ring.
    focus_ring()->SetColor(ash_color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));

    // Though the entirety of the header is focusable and behaves as a single
    // button, the focus ring is drawn as a circle around just the `chevron`.
    focus_ring()->SetPathGenerator(
        std::make_unique<CallbackPathGenerator>(base::BindRepeating(
            [](const views::View* chevron) {
              const float radius = chevron->width() / 2.f;
              gfx::RRectF path(gfx::RectF(chevron->bounds()), radius);
              if (base::i18n::IsRTL()) {
                // Manually adjust for flipped canvas in RTL.
                path.Offset(-chevron->parent()->width(), 0.f);
                path.Scale(-1.f, 1.f);
              }
              return path;
            },
            base::Unretained(chevron))));
  }

 private:
  void OnPressed() {
    holding_space_metrics::RecordDownloadsAction(
        holding_space_metrics::DownloadsAction::kClick);

    HoldingSpaceController::Get()->client()->OpenDownloads(base::DoNothing());
  }
};

}  // namespace

// DownloadsSection ------------------------------------------------------------

DownloadsSection::DownloadsSection(HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   /*supported_types=*/
                                   {HoldingSpaceItem::Type::kDownload,
                                    HoldingSpaceItem::Type::kNearbyShare},
                                   /*max_count=*/kMaxDownloads) {}

DownloadsSection::~DownloadsSection() = default;

const char* DownloadsSection::GetClassName() const {
  return "DownloadsSection";
}

std::unique_ptr<views::View> DownloadsSection::CreateHeader() {
  auto header = std::make_unique<Header>();
  header->SetPaintToLayer();
  header->layer()->SetFillsBoundsOpaquely(false);
  return header;
}

std::unique_ptr<views::View> DownloadsSection::CreateContainer() {
  return std::make_unique<HoldingSpaceItemChipsContainer>();
}

std::unique_ptr<HoldingSpaceItemView> DownloadsSection::CreateView(
    const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

}  // namespace ash
