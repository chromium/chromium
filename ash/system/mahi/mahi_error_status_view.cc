// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_error_status_view.h"

#include <memory>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/mahi_utils.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"

namespace ash {

namespace {

// Constants -------------------------------------------------------------------

constexpr auto kContentsPaddings =
    gfx::Insets::VH(/*vertical=*/40, /*horizontal=*/0);

constexpr int kErrorStatusViewPaddings = 8;

constexpr auto kImagePaddings = gfx::Insets::TLBR(/*top=*/0,
                                                  /*left=*/56,
                                                  /*bottom=*/16,
                                                  /*right=*/56);

constexpr gfx::Size kImagePreferredSize(/*width=*/200, /*height=*/100);

// TODO(b/319731776): Use panel bounds instead when the panel is resizable.
constexpr int kLabelMaximumWidth = 264;

constexpr auto kLabelPaddings =
    gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/24);

// ErrorContentsView -----------------------------------------------------------

class ErrorContentsView : public views::FlexLayoutView,
                          public MahiUiController::Delegate {
 public:
  explicit ErrorContentsView(MahiUiController* ui_controller)
      : MahiUiController::Delegate(ui_controller),
        ui_controller_(ui_controller) {
    // TODO(http://b/319731862): Set the image when the image resource is ready.
    views::Builder<views::FlexLayoutView>(this)
        .SetBorder(views::CreateEmptyBorder(kContentsPaddings))
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetOrientation(views::LayoutOrientation::kVertical)
        .AddChildren(
            views::Builder<views::ImageView>()
                .SetBorder(views::CreateEmptyBorder(kImagePaddings))
                .SetImage(ui::ResourceBundle::GetSharedInstance()
                              .GetThemedLottieImageNamed(
                                  IDR_MAHI_GENERAL_ERROR_STATUS_IMAGE))
                .SetPreferredSize(kImagePreferredSize),
            views::Builder<views::Label>()
                .AfterBuild(base::BindOnce([](views::Label* self) {
                  TypographyProvider::Get()->StyleLabel(
                      TypographyToken::kCrosBody2, *self);
                }))
                .CopyAddressTo(&error_status_text_)
                .SetBorder(views::CreateEmptyBorder(kLabelPaddings))
                .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
                .SetID(mahi_constants::ViewId::kErrorStatusLabel)
                .SetMultiLine(true)
                .SetMaximumWidth(kLabelMaximumWidth),
            views::Builder<views::Link>()
                .CopyAddressTo(&retry_link_)
                .SetForceUnderline(false)
                .SetID(mahi_constants::ViewId::kErrorStatusRetryLink)
                .SetText(l10n_util::GetStringUTF16(
                    IDS_ASH_MAHI_RETRY_LINK_LABEL_TEXT))
                .SetVisible(false))
        .BuildChildren();
  }

  ErrorContentsView(const ErrorContentsView&) = delete;
  ErrorContentsView& operator=(const ErrorContentsView&) = delete;
  ~ErrorContentsView() override = default;

 private:
  // MahiUiController::Delegate:
  views::View* GetView() override { return this; }

  bool GetViewVisibility(VisibilityState state) const override {
    // Always visible because its parent view controls visibility.
    return true;
  }

  void OnUpdated(const MahiUiUpdate& update) override {
    switch (update.type()) {
      case MahiUiUpdateType::kErrorReceived: {
        const MahiUiError& error = update.GetError();
        error_status_text_->SetText(l10n_util::GetStringUTF16(
            mahi_utils::GetErrorStatusViewTextId(error.status)));

        retry_link_->SetVisible(
            mahi_utils::CalculateRetryLinkVisible(error.status));
        retry_link_->SetCallback(
            retry_link_->GetVisible()
                ? base::BindRepeating(
                      [](MahiUiController* controller,
                         VisibilityState origin_state,
                         views::View* retry_link) {
                        controller->Retry(origin_state);
                        retry_link->GetViewAccessibility().AnnounceText(
                            l10n_util::GetStringUTF16(
                                IDS_ASH_MAHI_RETRY_LINK_CLICK_ACTIVATION_ACCESSIBLE_NAME));
                      },
                      ui_controller_, error.origin_state, retry_link_)
                : base::RepeatingClosure());
        return;
      }
      case MahiUiUpdateType::kAnswerLoaded:
      case MahiUiUpdateType::kContentsRefreshInitiated:
      case MahiUiUpdateType::kOutlinesLoaded:
      case MahiUiUpdateType::kQuestionAndAnswerViewNavigated:
      case MahiUiUpdateType::kQuestionPosted:
      case MahiUiUpdateType::kQuestionReAsked:
      case MahiUiUpdateType::kRefreshAvailabilityUpdated:
      case MahiUiUpdateType::kSummaryLoaded:
      case MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated:
      case MahiUiUpdateType::kSummaryAndOutlinesReloaded:
        return;
    }
  }

  const raw_ptr<MahiUiController> ui_controller_;

  raw_ptr<views::Label> error_status_text_ = nullptr;
  raw_ptr<views::Link> retry_link_ = nullptr;
};

}  // namespace

MahiErrorStatusView::MahiErrorStatusView(MahiUiController* ui_controller)
    : MahiUiController::Delegate(ui_controller) {
  CHECK(chromeos::features::IsMahiEnabled());

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  views::Builder<views::FlexLayoutView>(this)
      .SetBorder(views::CreateEmptyBorder(
          gfx::Insets(/*all=*/kErrorStatusViewPaddings)))
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetID(mahi_constants::ViewId::kErrorStatusView)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .AddChild(views::Builder<views::View>(
          std::make_unique<ErrorContentsView>(ui_controller)))
      .BuildChildren();
}

MahiErrorStatusView::~MahiErrorStatusView() = default;

views::View* MahiErrorStatusView::GetView() {
  return this;
}

bool MahiErrorStatusView::GetViewVisibility(VisibilityState state) const {
  switch (state) {
    case VisibilityState::kError:
      return true;
    case VisibilityState::kQuestionAndAnswer:
    case VisibilityState::kSummaryAndOutlines:
      return false;
  }
}

void MahiErrorStatusView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetClipPath(mahi_utils::GetCutoutClipPath(
      /*contents_size=*/GetContentsBounds().size()));
}

bool MahiErrorStatusView::DoesIntersectRect(const views::View* target,
                                            const gfx::Rect& rect) const {
  if (!mahi_utils::ShouldShowFeedbackButton()) {
    return views::ViewTargeterDelegate::DoesIntersectRect(target, rect);
  }

  auto contents_bounds = GetContentsBounds();
  contents_bounds.Outset(gfx::Outsets(kErrorStatusViewPaddings));

  return !rect.Intersects(mahi_utils::GetCornerCutoutRegion(contents_bounds));
}

BEGIN_METADATA(MahiErrorStatusView)
END_METADATA

}  // namespace ash
