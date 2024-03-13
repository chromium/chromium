// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_question_answer_view.h"

#include <string>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr gfx::Insets kInteriorMargin = gfx::Insets(8);
constexpr gfx::Insets kTextBubbleInteriorMargin = gfx::Insets::VH(8, 12);
constexpr int kBetweenChildSpacing = 8;
constexpr int kTextBubbleCornerRadius = 12;

std::vector<std::u16string> sample_question_list = {
    u"What zibbleblorp of snazzlefrack wumpusplump do you believe "
    u"grumpenschnark flibberflabbersquish to groggletwist with zorpzorp in the "
    u"glippitygloop of blazzleblarf?",
    u"Would you rather eat a sniggle for breakfast or a womble for lunch?",
    u"What glimjams zorgleflumbers the snizzlewumps?",
    u"If a grumple could flibberflab, would it choose a snoozle or a "
    u"wizzleboop?",
    u"Short question?",
};

std::vector<std::u16string> sample_answer_list = {
    u"Flippity floppity snazzlefrack! The wumpusplump zorgledorf wibbledorf "
    u"into the flibberflabbersquish, causing a kerfuffle of zorpzorp "
    u"proportions!",
    u"I'd go with a sniggle! They say freshly picked sniggles have a "
    u"satisfying squish and a surprisingly tangy floofle flavor.",
    u"The flibberzorps often quibble with the zingledoodles over squanching "
    u"flumjabbles.",
    u"That depends entirely on the grumple's mood! A cheerful grumple would "
    u"certainly flibberflab with a wizzleboop, as the colors are known to "
    u"spark joy. However,  a grumpy grumple might prefer the calming tones of "
    u"a snoozle for its flibberflabbing.",
    u"Short answer. (Last example)",
};

// Creates a text bubble that will be populated with `text` and styled
// to be a question or answer based on `is_question`.
std::unique_ptr<views::View> CreateTextBubble(const std::u16string& text,
                                              bool is_question) {
  return views::Builder<views::FlexLayoutView>()
      .SetInteriorMargin(kTextBubbleInteriorMargin)
      .SetBackground(views::CreateThemedRoundedRectBackground(
          is_question ? cros_tokens::kCrosSysSystemPrimaryContainer
                      : cros_tokens::kCrosSysSystemOnBase,
          gfx::RoundedCornersF(kTextBubbleCornerRadius)))
      .SetMainAxisAlignment(is_question ? views::LayoutAlignment::kEnd
                                        : views::LayoutAlignment::kStart)
      .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
        layout->SetProperty(
            views::kFlexBehaviorKey,
            views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                     views::MaximumFlexSizeRule::kPreferred,
                                     /*adjust_height_for_width=*/true));
      }))
      .AddChildren(
          views::Builder<views::Label>()
              .SetMultiLine(true)
              .CustomConfigure(base::BindOnce([](views::Label* label) {
                label->SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kPreferred,
                                       /*adjust_height_for_width=*/true));
              }))
              .SetText(text)
              .SetTooltipText(text)
              .SetHorizontalAlignment(is_question ? gfx::ALIGN_RIGHT
                                                  : gfx::ALIGN_LEFT)
              .SetEnabledColorId(
                  is_question ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                              : cros_tokens::kCrosSysOnSurface)
              .SetAutoColorReadabilityEnabled(false)
              .SetSubpixelRenderingEnabled(false)
              .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                  TypographyToken::kCrosBody2)))
      .Build();
}

}  // namespace

MahiQuestionAnswerView::MahiQuestionAnswerView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetInteriorMargin(kInteriorMargin);
  SetIgnoreDefaultMainAxisMargins(true);
  SetCollapseMargins(true);
  SetDefault(views::kMarginsKey, gfx::Insets::VH(kBetweenChildSpacing, 0));
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded));
}

void MahiQuestionAnswerView::CreateSampleQuestionAnswer() {
  static size_t qa_index = 0;

  // Return if there are no more sample questions.
  if (qa_index >= sample_question_list.size()) {
    return;
  }

  AddChildView(
      CreateTextBubble(sample_question_list[qa_index], /*is_question=*/true));
  AddChildView(
      CreateTextBubble(sample_answer_list[qa_index], /*is_question=*/false));

  qa_index++;
}

MahiQuestionAnswerView::~MahiQuestionAnswerView() = default;

BEGIN_METADATA(MahiQuestionAnswerView)
END_METADATA

}  // namespace ash
