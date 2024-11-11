// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/summary_outlines_elucidation_section.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_animation_utils.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/resources/grit/mahi_resources.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int64_t kSectionHeaderChildSpacing = 4;
constexpr int64_t kSectionHeaderIconSize = 16;
constexpr int64_t kSectionChildSpacing = 8;
constexpr int kTextLabelDefaultMaximumWidth =
    mahi_constants::kPanelDefaultWidth -
    mahi_constants::kPanelBorderAndPadding -
    mahi_constants::kSummaryOutlinesElucidationSectionPadding.width();

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

  // TODO(b/330643995): Show the section header once other sections are
  // available.
  view->SetVisible(false);

  return view;
}

}  // namespace

SummaryOutlinesElucidationSection::SummaryOutlinesElucidationSection(
    MahiUiController* ui_controller)
    : MahiUiController::Delegate(ui_controller), ui_controller_(ui_controller) {
  CHECK(ui_controller_);

  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetInsideBorderInsets(
      mahi_constants::kSummaryOutlinesElucidationSectionPadding);
  SetBetweenChildSpacing(kSectionChildSpacing);

  AddChildView(CreateSectionHeader(chromeos::kMahiSummarizeIcon,
                                   IDS_MAHI_PANEL_SUMMARY_SECTION_NAME));

  AddChildView(
      views::Builder<views::AnimatedImageView>()
          .CopyAddressTo(&summary_or_elucidation_loading_animated_image_)
          .SetID(mahi_constants::ViewId::kSummaryLoadingAnimatedImage)
          .SetAccessibleName(
              l10n_util::GetStringUTF16(IDS_ASH_MAHI_LOADING_ACCESSIBLE_NAME))
          .SetAnimatedImage(mahi_animation_utils::GetLottieAnimationData(
              IDR_MAHI_LOADING_SUMMARY_ANIMATION))
          .Build());

  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&indicator_label_)
                   .SetVisible(false)
                   .SetID(mahi_constants::ViewId::kSummaryElucidationIndicator)
                   .SetSelectable(false)
                   .SetMultiLine(false)
                   .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .AfterBuild(base::BindOnce([](views::Label* self) {
                     TypographyProvider::Get()->StyleLabel(
                         TypographyToken::kCrosButton2, *self);
                   }))
                   .Build());

  AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&summary_or_elucidation_label_)
          .SetVisible(false)
          .SetID(mahi_constants::ViewId::kSummaryLabel)
          .SetSelectable(true)
          .SetMultiLine(true)
          // TODO(crbug.com/40233803): Multiline label right now doesn't
          // work well with `FlexLayout`. The size constraint is not
          // passed down from the views tree in the first round of layout,
          // so we impose a maximum width constraint so that the first
          // layout handle the width and height constraint correctly.
          .SetMaximumWidth(kTextLabelDefaultMaximumWidth)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .AfterBuild(base::BindOnce([](views::Label* self) {
            TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                                  *self);
          }))
          .Build());

  // TODO(b/330643995): Show the outlines section once it is ready.
  auto* outlines_section_container = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetID(mahi_constants::ViewId::kOutlinesSectionContainer)
          .SetVisible(false)
          .Build());

  outlines_section_container->AddChildView(CreateSectionHeader(
      chromeos::kMahiOutlinesIcon, IDS_MAHI_PANEL_OUTLINES_SECTION_NAME));

  outlines_section_container->AddChildView(
      views::Builder<views::AnimatedImageView>()
          .CopyAddressTo(&outlines_loading_animated_image_)
          .SetID(mahi_constants::ViewId::kOutlinesLoadingAnimatedImage)
          .SetAnimatedImage(mahi_animation_utils::GetLottieAnimationData(
              IDR_MAHI_LOADING_OUTLINES_ANIMATION))
          .SetAccessibleName(
              l10n_util::GetStringUTF16(IDS_ASH_MAHI_LOADING_ACCESSIBLE_NAME))
          .Build());

  outlines_section_container->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .CopyAddressTo(&outlines_container_)
          .SetID(mahi_constants::ViewId::kOutlinesContainer)
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetVisible(false)
          .Build());
}

SummaryOutlinesElucidationSection::~SummaryOutlinesElucidationSection() =
    default;

views::View* SummaryOutlinesElucidationSection::GetView() {
  return this;
}

bool SummaryOutlinesElucidationSection::GetViewVisibility(
    VisibilityState state) const {
  switch (state) {
    case VisibilityState::kError:
    case VisibilityState::kQuestionAndAnswer:
      return false;
    case VisibilityState::kSummaryAndOutlinesAndElucidation:
      return true;
  }
}

void SummaryOutlinesElucidationSection::OnUpdated(const MahiUiUpdate& update) {
  switch (update.type()) {
    case MahiUiUpdateType::kContentsRefreshInitiated:
    case MahiUiUpdateType::kSummaryAndOutlinesReloaded:
      indicator_label_->SetVisible(false);
      LoadContentForDisplay(ContentType::kSummaryAndOutline);
      return;
    case MahiUiUpdateType::kOutlinesLoaded:
      HandleOutlinesLoaded(update.GetOutlines());
      return;
    case MahiUiUpdateType::kPanelBoundsChanged: {
      const gfx::Rect& panel_bounds = update.GetPanelBounds();
      // If the width of the panel has changed, update the maximum size of the
      // text.
      if (summary_or_elucidation_label_ != nullptr &&
          summary_or_elucidation_label_->GetMaximumWidth() !=
              panel_bounds.width()) {
        summary_or_elucidation_label_->SetMaximumWidth(
            panel_bounds.width() - mahi_constants::kPanelBorderAndPadding -
            mahi_constants::kSummaryOutlinesElucidationSectionPadding.width());
      }
      return;
    }
    case MahiUiUpdateType::kSummaryLoaded:
      HandleSummaryOrElucidationLoaded(update.GetSummary());
      base::UmaHistogramTimes(mahi_constants::kSummaryLoadingTimeHistogramName,
                              base::Time::Now() - start_loading_time_);
      return;
    case MahiUiUpdateType::kElucidationRequested:
      indicator_label_->SetVisible(false);
      LoadContentForDisplay(ContentType::kElucidation);
      return;
    case MahiUiUpdateType::kElucidationLoaded:
      HandleSummaryOrElucidationLoaded(update.GetElucidation());
      base::UmaHistogramTimes(
          mahi_constants::kElucidationLoadingTimeHistogramName,
          base::Time::Now() - start_loading_time_);
      return;
    case MahiUiUpdateType::kAnswerLoaded:
    case MahiUiUpdateType::kErrorReceived:
    case MahiUiUpdateType::kQuestionAndAnswerViewNavigated:
    case MahiUiUpdateType::kQuestionPosted:
    case MahiUiUpdateType::kQuestionReAsked:
    case MahiUiUpdateType::kRefreshAvailabilityUpdated:
    case MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated:
      return;
  }
}

void SummaryOutlinesElucidationSection::AddedToWidget() {
  // When the view is first constructed, we need to wait until the view is added
  // to the widget to announce loading state.
  if (summary_or_elucidation_loading_animated_image_->GetVisible() ||
      outlines_loading_animated_image_->GetVisible()) {
    GetViewAccessibility().AnnounceText(
        l10n_util::GetStringUTF16(IDS_ASH_MAHI_LOADING_ACCESSIBLE_NAME));
  }
}

void SummaryOutlinesElucidationSection::HandleOutlinesLoaded(
    const std::vector<chromeos::MahiOutline>& outlines) {
  outlines_container_->RemoveAllChildViews();
  for (auto outline : outlines) {
    outlines_container_->AddChildView(
        views::Builder<views::Label>()
            .SetText(outline.outline_content)
            .SetSelectable(true)
            .SetMultiLine(true)
            .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .AfterBuild(base::BindOnce([](views::Label* self) {
              TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                                    *self);
            }))
            .Build());
  }

  outlines_loading_animated_image_->Stop();
  outlines_loading_animated_image_->SetVisible(false);
  // TODO(b/330643995): Show the outlines section once it is ready. Note that
  // when enabling outlines, we need to make sure that the state of fully
  // loaded is announced in accessibility (similar to what is done in
  // `HandleSummaryLoaded()`).
  outlines_container_->SetVisible(false);

  // TODO(b/333916944): Add metrics recording the outline loading animation time
  // here.
}

void SummaryOutlinesElucidationSection::HandleSummaryOrElucidationLoaded(
    const std::u16string& result_text) {
  indicator_label_->SetVisible(true);
  summary_or_elucidation_label_->SetVisible(true);
  summary_or_elucidation_label_->SetText(result_text);
  summary_or_elucidation_loading_animated_image_->Stop();
  summary_or_elucidation_loading_animated_image_->SetVisible(false);

  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_LOADED_ACCESSIBLE_NAME));
}

void SummaryOutlinesElucidationSection::LoadContentForDisplay(
    ContentType content_type) {
  if (!chromeos::MahiManager::Get()) {
    CHECK_IS_TEST();
    return;
  }

  if (summary_or_elucidation_label_->GetVisible()) {
    summary_or_elucidation_label_->SetVisible(false);
    summary_or_elucidation_loading_animated_image_->SetVisible(true);
  }

  if (outlines_container_->GetVisible()) {
    outlines_container_->SetVisible(false);
    outlines_loading_animated_image_->SetVisible(true);
  }

  // Plays loading animation before summary and outlines are loaded.
  summary_or_elucidation_loading_animated_image_->Play(
      mahi_animation_utils::GetLottiePlaybackConfig(
          *summary_or_elucidation_loading_animated_image_->animated_image()
               ->skottie(),
          IDR_MAHI_LOADING_SUMMARY_ANIMATION));
  outlines_loading_animated_image_->Play(
      mahi_animation_utils::GetLottiePlaybackConfig(
          *outlines_loading_animated_image_->animated_image()->skottie(),
          IDR_MAHI_LOADING_OUTLINES_ANIMATION));

  GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_LOADING_ACCESSIBLE_NAME));

  start_loading_time_ = base::Time::Now();

  switch (content_type) {
    case ContentType::kSummaryAndOutline: {
      indicator_label_->SetText(
          l10n_util::GetStringUTF16(IDS_MAHI_SUMMARIZE_INDICATOR_LABEL));
      ui_controller_->UpdateSummaryAndOutlines();
      return;
    }
    case ContentType::kElucidation: {
      indicator_label_->SetText(
          l10n_util::GetStringUTF16(IDS_MAHI_SIMPLIFY_INDICATOR_LABEL));
      ui_controller_->UpdateElucidation();
      return;
    }
  }
}

BEGIN_METADATA(SummaryOutlinesElucidationSection)
END_METADATA

}  // namespace ash
