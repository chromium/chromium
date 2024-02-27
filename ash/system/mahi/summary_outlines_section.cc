// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/summary_outlines_section.h"

#include <memory>
#include <string>

#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "base/check_is_test.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int64_t kSectionHeaderChildSpacing = 4;
constexpr int64_t kSectionHeaderIconSize = 20;
constexpr gfx::Insets kSectionPadding = gfx::Insets::TLBR(8, 8, 16, 8);
constexpr int64_t kSectionChildSpacing = 8;
constexpr int64_t kSectionCornerRadius = 16;

std::unique_ptr<views::View> CreateSectionHeader(const gfx::VectorIcon& icon,
                                                 int name_id) {
  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  view->SetBetweenChildSpacing(kSectionHeaderChildSpacing);

  view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          icon, cros_tokens::kCrosSysOnSurface, kSectionHeaderIconSize)));

  auto label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(name_id));
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *label);
  view->AddChildView(std::move(label));
  return view;
}

}  // namespace

SummaryOutlinesSection::SummaryOutlinesSection() {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetInsideBorderInsets(kSectionPadding);
  SetBetweenChildSpacing(kSectionChildSpacing);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kSectionCornerRadius));

  AddChildView(CreateSectionHeader(chromeos::kMahiSummarizeIcon,
                                   IDS_MAHI_PANEL_SUMMARY_SECTION_NAME));

  auto* summary_label = AddChildView(std::make_unique<views::Label>());
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                        *summary_label);
  summary_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  summary_label->SetID(mahi_constants::ViewId::kSummaryLabel);
  summary_label->SetMultiLine(true);
  summary_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->GetSummary(base::BindOnce(
        [](base::WeakPtr<SummaryOutlinesSection> parent,
           views::Label* summary_label, std::u16string summary_text) {
          if (!parent) {
            return;
          }

          summary_label->SetText(summary_text);
        },
        weak_ptr_factory_.GetWeakPtr(), summary_label));
  } else {
    CHECK_IS_TEST();
  }

  AddChildView(CreateSectionHeader(chromeos::kMahiOutlinesIcon,
                                   IDS_MAHI_PANEL_OUTLINES_SECTION_NAME));
}

SummaryOutlinesSection::~SummaryOutlinesSection() = default;

BEGIN_METADATA(SummaryOutlinesSection)
END_METADATA

}  // namespace ash
