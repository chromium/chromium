// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/suggestion_window_view.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/assistive_window_properties.h"
#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "chrome/browser/chromeos/input_method/ui/border_factory.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_color_id.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_properties.h"

namespace ui {
namespace ime {

namespace {

bool ShouldHighlight(const views::Button& button) {
  return button.GetState() == views::Button::STATE_HOVERED ||
         button.GetState() == views::Button::STATE_PRESSED;
}

// TODO(b/1101669): Create abstract HighlightableButton for learn_more button,
// setting_link_, suggestion_view and undo_view.
void SetHighlighted(views::View& view, bool highlighted) {
  if (!!view.background() != highlighted) {
    view.SetBackground(highlighted
                           ? views::CreateSolidBackground(kButtonHighlightColor)
                           : nullptr);
  }
}

}  // namespace

// static
SuggestionWindowView* SuggestionWindowView::Create(
    gfx::NativeView parent,
    AssistiveDelegate* delegate) {
  auto* const view = new SuggestionWindowView(parent, delegate);
  views::Widget* const widget =
      views::BubbleDialogDelegateView::CreateBubble(view);
  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);
  return view;
}

std::unique_ptr<views::NonClientFrameView>
SuggestionWindowView::CreateNonClientFrameView(views::Widget* widget) {
  std::unique_ptr<views::NonClientFrameView> frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(GetBorderForWindow(WindowBorderType::Suggestion));
  return frame;
}

// TODO(crbug/1099116): Add test for ButtonPressed.
void SuggestionWindowView::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  DCHECK(sender);
  AssistiveWindowButton button;
  if (sender->parent() == candidate_area_) {
    button.id = ui::ime::ButtonId::kSuggestion;
    button.index = candidate_area_->GetIndexOf(sender);
  } else {
    DCHECK_EQ(learn_more_button_, sender);
    button.id = ui::ime::ButtonId::kLearnMore;
    button.window_type = ui::ime::AssistiveWindowType::kEmojiSuggestion;
  }
  delegate_->AssistiveWindowButtonClicked(button);
}

void SuggestionWindowView::Show(const SuggestionDetails& details) {
  ResizeCandidateArea(1);
  auto* const candidate =
      static_cast<SuggestionView*>(candidate_area_->children().front());
  candidate->SetView(details);
  if (details.show_setting_link)
    candidate->SetMinWidth(setting_link_->GetPreferredSize().width());

  setting_link_->SetVisible(details.show_setting_link);

  MakeVisible();
}

void SuggestionWindowView::ShowMultipleCandidates(
    const chromeos::AssistiveWindowProperties& properties) {
  const std::vector<base::string16>& candidates = properties.candidates;
  ResizeCandidateArea(candidates.size());
  for (size_t i = 0; i < candidates.size(); ++i) {
    auto* const candidate =
        static_cast<SuggestionView*>(candidate_area_->children()[i]);
    if (properties.show_indices)
      candidate->SetViewWithIndex(base::FormatNumber(i + 1), candidates[i]);
    else
      candidate->SetView({.text = candidates[i]});
  }

  learn_more_button_->SetVisible(properties.show_setting_link);

  MakeVisible();
}

void SuggestionWindowView::SetButtonHighlighted(
    const AssistiveWindowButton& button,
    bool highlighted) {
  if (button.id == ButtonId::kSuggestion) {
    const views::View::Views& candidates = candidate_area_->children();
    if (button.index < candidates.size()) {
      SetCandidateHighlighted(
          static_cast<SuggestionView*>(candidates[button.index]), highlighted);
    }
  } else if (button.id == ButtonId::kSmartInputsSettingLink) {
    SetHighlighted(*setting_link_, highlighted);
  } else if (button.id == ButtonId::kLearnMore) {
    SetHighlighted(*learn_more_button_, highlighted);
  }
}

void SuggestionWindowView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();

  learn_more_button_->SetBorder(views::CreatePaddedBorder(
      views::CreateSolidSidedBorder(
          1, 0, 0, 0,
          GetNativeTheme()->GetSystemColor(
              ui::NativeTheme::kColorId_FootnoteContainerBorder)),
      views::LayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_VECTOR_IMAGE_BUTTON)));

  // TODO(crbug/1099044): Update and use cros colors.
  constexpr SkColor kSecondaryIconColor = gfx::kGoogleGrey500;
  learn_more_button_->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(vector_icons::kHelpOutlineIcon,
                            kSecondaryIconColor));
}

SuggestionWindowView::SuggestionWindowView(gfx::NativeView parent,
                                           AssistiveDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(parent);

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetCanActivate(false);
  set_parent_window(parent);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  candidate_area_ = AddChildView(std::make_unique<views::View>());
  candidate_area_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  setting_link_ = AddChildView(std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_SUGGESTION_LEARN_MORE)));
  setting_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // TODO(crbug/1102215): Implement proper UI layout using Insets constant.
  constexpr gfx::Insets insets(0, kPadding, kPadding, kPadding);
  setting_link_->SetBorder(views::CreateEmptyBorder(insets));
  constexpr int kSettingLinkFontSize = 11;
  setting_link_->SetFontList(gfx::FontList({kFontStyle}, gfx::Font::ITALIC,
                                           kSettingLinkFontSize,
                                           gfx::Font::Weight::NORMAL));
  const auto on_setting_link_clicked = [](AssistiveDelegate* delegate) {
    delegate->AssistiveWindowButtonClicked(
        {.id = ButtonId::kSmartInputsSettingLink});
  };
  setting_link_->set_callback(
      base::BindRepeating(on_setting_link_clicked, delegate_));
  setting_link_->SetVisible(false);

  learn_more_button_ = AddChildView(std::make_unique<views::ImageButton>(this));
  learn_more_button_->SetImageHorizontalAlignment(
      views::ImageButton::ALIGN_CENTER);
  learn_more_button_->SetImageVerticalAlignment(
      views::ImageButton::ALIGN_MIDDLE);
  learn_more_button_->SetFocusForPlatform();
  learn_more_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  const auto update_button_highlight = [](views::Button* button) {
    SetHighlighted(*button, ShouldHighlight(*button));
  };
  auto subscription =
      learn_more_button_->AddStateChangedCallback(base::BindRepeating(
          update_button_highlight, base::Unretained(learn_more_button_)));
  subscriptions_.insert({learn_more_button_, std::move(subscription)});
  learn_more_button_->SetVisible(false);
}

SuggestionWindowView::~SuggestionWindowView() = default;

void SuggestionWindowView::ResizeCandidateArea(size_t size) {
  if (highlighted_candidate_)
    SetCandidateHighlighted(highlighted_candidate_, false);

  const views::View::Views& candidates = candidate_area_->children();
  while (candidates.size() > size) {
    subscriptions_.erase(
        candidate_area_->RemoveChildViewT(candidates.back()).get());
  }

  while (candidates.size() < size) {
    auto* const candidate =
        candidate_area_->AddChildView(std::make_unique<SuggestionView>(this));
    auto subscription = candidate->AddStateChangedCallback(base::BindRepeating(
        [](SuggestionWindowView* window, SuggestionView* button) {
          window->SetCandidateHighlighted(button, ShouldHighlight(*button));
        },
        base::Unretained(this), base::Unretained(candidate)));
    subscriptions_.insert({candidate, std::move(subscription)});
  }
}

void SuggestionWindowView::MakeVisible() {
  candidate_area_->SetVisible(true);
  SizeToContents();
}

void SuggestionWindowView::SetCandidateHighlighted(SuggestionView* candidate,
                                                   bool highlighted) {
  DCHECK(candidate);
  DCHECK_EQ(candidate_area_, candidate->parent());

  // Can't highlight a highlighted candidate, or unhighlight an unhighlighted
  // one.
  if (highlighted == (candidate == highlighted_candidate_))
    return;

  if (highlighted && highlighted_candidate_)
    highlighted_candidate_->SetHighlighted(false);
  candidate->SetHighlighted(highlighted);
  highlighted_candidate_ = highlighted ? candidate : nullptr;
}

BEGIN_METADATA(SuggestionWindowView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ime
}  // namespace ui
