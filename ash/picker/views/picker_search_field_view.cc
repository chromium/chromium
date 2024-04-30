// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <string>
#include <string_view>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

constexpr auto kSearchFieldBorderInsets = gfx::Insets::VH(0, 16);
constexpr auto kSearchFieldVerticalPadding = gfx::Insets::VH(6, 0);
constexpr auto kClearButtonHorizontalMargin = gfx::Insets::VH(0, 8);
constexpr int kClearButtonSizeDip = 20;

// TODO: b/331285414 - Finalize the search field placeholder text.
std::u16string GetSearchFieldPlaceholderText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_PICKER_SEARCH_FIELD_PLACEHOLDER_TEXT);
#else
  return l10n_util::GetStringUTF16(
      IDS_PICKER_ZERO_STATE_SEARCH_FIELD_PLACEHOLDER_TEXT);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

class ClearSearchFieldImageButton : public views::ImageButton {
  METADATA_HEADER(ClearSearchFieldImageButton, views::ImageButton)

 public:
  ClearSearchFieldImageButton() {
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                                 /*highlight_on_hover=*/true);

    SetPreferredSize(gfx::Size(kClearButtonSizeDip, kClearButtonSizeDip));
    SetImageHorizontalAlignment(ALIGN_CENTER);
    SetImageVerticalAlignment(ALIGN_MIDDLE);
    SetImageModel(views::ImageButton::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(views::kIcCloseIcon,
                                                 kColorAshButtonIconColor,
                                                 kClearButtonSizeDip));

    views::InstallCircleHighlightPathGenerator(this);
  }
  ClearSearchFieldImageButton(const ClearSearchFieldImageButton&) = delete;
  ClearSearchFieldImageButton& operator=(const ClearSearchFieldImageButton&) =
      delete;

  ~ClearSearchFieldImageButton() override {}
};

BEGIN_METADATA(ClearSearchFieldImageButton)
END_METADATA

BEGIN_VIEW_BUILDER(ASH_EXPORT, ClearSearchFieldImageButton, views::ImageButton)
END_VIEW_BUILDER

}  // namespace
}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::ClearSearchFieldImageButton)

namespace ash {

PickerSearchFieldView::PickerSearchFieldView(
    SearchCallback search_callback,
    PickerKeyEventHandler* key_event_handler,
    PickerPerformanceMetrics* performance_metrics)
    : search_callback_(std::move(search_callback)),
      key_event_handler_(key_event_handler),
      performance_metrics_(performance_metrics) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  views::Builder<PickerSearchFieldView>(this)
      .SetProperty(views::kMarginsKey, kSearchFieldVerticalPadding)
      .AddChild(
          views::Builder<views::Textfield>()
              .CopyAddressTo(&textfield_)
              .SetProperty(views::kElementIdentifierKey,
                           kPickerSearchFieldTextfieldElementId)
              .SetController(this)
              .SetBorder(views::CreateEmptyBorder(kSearchFieldBorderInsets))
              .SetBackgroundColor(SK_ColorTRANSPARENT)
              .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                  TypographyToken::kCrosBody2))
              .SetPlaceholderText(GetSearchFieldPlaceholderText())
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded))
              // TODO(b/309706053): Replace this once the strings are finalized.
              .SetAccessibleName(u"placeholder"))
      .AddChild(
          views::Builder<ClearSearchFieldImageButton>()
              .CopyAddressTo(&clear_button_)
              // `base::Unretained` is safe here since the search field is owned
              // by this class.
              .SetCallback(base::BindRepeating(
                  &PickerSearchFieldView::ClearButtonPressed,
                  base::Unretained(this)))
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred))
              .SetProperty(views::kMarginsKey, kClearButtonHorizontalMargin)
              .SetVisible(false)
              // TODO(b/309706053): Replace this once the strings are finalized.
              .SetAccessibleName(u"placeholder"))
      .BuildChildren();
}

PickerSearchFieldView::~PickerSearchFieldView() = default;

void PickerSearchFieldView::RequestFocus() {
  textfield_->RequestFocus();
}

void PickerSearchFieldView::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void PickerSearchFieldView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void PickerSearchFieldView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  performance_metrics_->MarkContentsChanged();

  // Show the clear button only when the query is not empty.
  clear_button_->SetVisible(!new_contents.empty());

  search_callback_.Run(new_contents);
}

bool PickerSearchFieldView::HandleKeyEvent(views::Textfield* sender,
                                           const ui::KeyEvent& key_event) {
  return key_event_handler_->HandleKeyEvent(key_event);
}

void PickerSearchFieldView::OnWillChangeFocus(View* focused_before,
                                              View* focused_now) {}

void PickerSearchFieldView::OnDidChangeFocus(View* focused_before,
                                             View* focused_now) {
  if (focused_now == textfield_) {
    performance_metrics_->MarkInputFocus();
  }
}

void PickerSearchFieldView::SetPlaceholderText(
    std::u16string_view new_placeholder_text) {
  textfield_->SetPlaceholderText(std::u16string(new_placeholder_text));
}

void PickerSearchFieldView::SetTextfieldActiveDescendant(views::View* view) {
  if (view) {
    textfield_->GetViewAccessibility().SetActiveDescendant(*view);
  } else {
    textfield_->GetViewAccessibility().ClearActiveDescendant();
  }

  textfield_->NotifyAccessibilityEvent(
      ax::mojom::Event::kActiveDescendantChanged, true);
}

std::u16string_view PickerSearchFieldView::GetQueryText() const {
  return textfield_->GetText();
}

void PickerSearchFieldView::SetQueryText(std::u16string text) {
  textfield_->SetText(std::move(text));
}

void PickerSearchFieldView::ClearButtonPressed() {
  textfield_->SetText(u"");
  ContentsChanged(textfield_, u"");
}

BEGIN_METADATA(PickerSearchFieldView)
END_METADATA

}  // namespace ash
