// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/suggestions_section.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_ui.h"
#include "ash/system/holding_space/holding_space_util.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Header ----------------------------------------------------------------------

class Header : public views::Button {
 public:
  Header() {
    // Layout/Properties.
    views::Builder<views::Button>(this)
        .SetID(kHoldingSpaceSuggestionsSectionHeaderId)
        .SetAccessibleName(
            l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_SUGGESTIONS_TITLE))
        .SetCallback(
            base::BindRepeating(&Header::OnPressed, base::Unretained(this)))
        .SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            kHoldingSpaceSectionHeaderSpacing))
        .AddChildren(
            holding_space_ui::CreateSuggestionsSectionHeaderLabel(
                IDS_ASH_HOLDING_SPACE_SUGGESTIONS_TITLE)
                .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                .SetProperty(views::kBoxLayoutFlexKey,
                             views::BoxLayoutFlexSpecification()),
            views::Builder<views::ImageView>().CopyAddressTo(&chevron_).SetID(
                kHoldingSpaceSuggestionsChevronIconId))
        .BuildChildren();

    // Though the entirety of the header is focusable and behaves as a single
    // button, the focus ring is drawn as a circle around just the `chevron_`.
    views::FocusRing::Get(this)->SetPathGenerator(
        holding_space_util::CreateHighlightPathGenerator(base::BindRepeating(
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
            base::Unretained(chevron_))));
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

    auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(prefs);

    // NOTE: The binding of this callback is scoped to `pref_change_registrar_`,
    // which is owned by `this`, so it is safe to bind with an unretained raw
    // pointer.
    holding_space_prefs::AddSuggestionsExpandedChangedCallback(
        pref_change_registrar_.get(),
        base::BindRepeating(&Header::UpdateState, base::Unretained(this)));

    // Initialize state.
    UpdateState();
  }

 private:
  void OnPressed() {
    auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
    bool expanded = holding_space_prefs::IsSuggestionsExpanded(prefs);
    holding_space_prefs::SetSuggestionsExpanded(prefs, !expanded);
    UpdateExpandedCollapsedAccessibleState(!expanded);

    holding_space_metrics::RecordSuggestionsAction(
        expanded ? holding_space_metrics::SuggestionsAction::kCollapse
                 : holding_space_metrics::SuggestionsAction::kExpand);
  }

  void UpdateState() {
    // Chevron.
    auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
    bool expanded = holding_space_prefs::IsSuggestionsExpanded(prefs);
    chevron_->SetImage(ui::ImageModel::FromVectorIcon(
        expanded ? kChevronUpSmallIcon : kChevronDownSmallIcon,
        kColorAshIconColorSecondary, kHoldingSpaceSectionChevronIconSize));

    UpdateExpandedCollapsedAccessibleState(expanded);
  }

  void UpdateExpandedCollapsedAccessibleState(bool expanded) const {
    if (expanded) {
      GetViewAccessibility().SetIsExpanded();
    } else {
      GetViewAccessibility().SetIsCollapsed();
    }
  }

  // Owned by view hierarchy.
  raw_ptr<views::ImageView> chevron_ = nullptr;

  // The user can expand and collapse the suggestions section by activating the
  // section header. This registrar is associated with the active user pref
  // service and notifies the header of changes to the user's preference.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace

// SuggestionsSection ----------------------------------------------------------

SuggestionsSection::SuggestionsSection(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   HoldingSpaceSectionId::kSuggestions) {
  SetID(kHoldingSpaceSuggestionsSectionId);

  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  // NOTE: The binding of this callback is scoped to `pref_change_registrar_`,
  // which is owned by `this`, so it is safe to bind with an unretained raw
  // pointer.
  holding_space_prefs::AddSuggestionsExpandedChangedCallback(
      pref_change_registrar_.get(),
      base::BindRepeating(&SuggestionsSection::OnExpandedChanged,
                          base::Unretained(this)));
}

SuggestionsSection::~SuggestionsSection() = default;

std::unique_ptr<views::View> SuggestionsSection::CreateHeader() {
  return std::make_unique<Header>();
}

std::unique_ptr<views::View> SuggestionsSection::CreateContainer() {
  return views::Builder<views::View>()
      .SetID(kHoldingSpaceSuggestionsSectionContainerId)
      .SetLayoutManager(std::make_unique<SimpleGridLayout>(
          kHoldingSpaceChipCountPerRow,
          /*column_spacing=*/kHoldingSpaceSectionContainerChildSpacing,
          /*row_spacing=*/kHoldingSpaceSectionContainerChildSpacing))
      .Build();
}

std::unique_ptr<HoldingSpaceItemView> SuggestionsSection::CreateView(
    const HoldingSpaceItem* item) {
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

bool SuggestionsSection::IsExpanded() {
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  return holding_space_prefs::IsSuggestionsExpanded(prefs);
}

BEGIN_METADATA(SuggestionsSection)
END_METADATA

}  // namespace ash
