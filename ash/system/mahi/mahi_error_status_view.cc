// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_error_status_view.h"

#include <memory>
#include <string>
#include <variant>

#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "base/notreached.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Constants -------------------------------------------------------------------

constexpr auto kContentsPaddings =
    gfx::Insets::VH(/*vertical=*/40, /*horizontal=*/0);

constexpr auto kErrorStatusViewPaddings = gfx::Insets(/*all=*/8);

constexpr auto kImagePaddings = gfx::Insets::TLBR(/*top=*/0,
                                                  /*left=*/56,
                                                  /*bottom=*/16,
                                                  /*right=*/56);

constexpr gfx::Size kImagePreferredSize(/*width=*/200, /*height=*/73);

constexpr int kLabelMaximumWidth = 264;

constexpr auto kLabelPaddings =
    gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/24);

// Helpers ---------------------------------------------------------------------

// Returns the label text for `error`. NOTE: `kLowQuota` triggers a warning that
// is not presented in `MahiErrorStatusView`.
// TODO(http://b/319731862): Add UI strings.
std::u16string GetErrorLabelText(chromeos::MahiResponseStatus error) {
  switch (error) {
    case chromeos::MahiResponseStatus::kCantFindOutputData:
    case chromeos::MahiResponseStatus::kContentExtractionError:
    case chromeos::MahiResponseStatus::kInappropriate:
    case chromeos::MahiResponseStatus::kUnknownError:
      return u"Something went wrong";
    case chromeos::MahiResponseStatus::kQuotaLimitHit:
      return u"Due to high request volume, your access is temporary limited. "
             u"Try again tomorrow.";
    case chromeos::MahiResponseStatus::kResourceExhausted:
      return u"Can't use right now. Try again later";
    case chromeos::MahiResponseStatus::kLowQuota:
    case chromeos::MahiResponseStatus::kSuccess:
      NOTREACHED_NORETURN();
  }
}

// ErrorContentsView -----------------------------------------------------------

class ErrorContentsView : public views::FlexLayoutView,
                          public MahiUiController::Observer {
 public:
  explicit ErrorContentsView(MahiUiController* ui_controller)
      : MahiUiController::Observer(ui_controller) {
    // TODO(http://b/319731862): Set the image when the image resource is ready.
    views::Builder<views::FlexLayoutView>(this)
        .SetBorder(views::CreateEmptyBorder(kContentsPaddings))
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetOrientation(views::LayoutOrientation::kVertical)
        .AddChildren(
            views::Builder<views::ImageView>()
                .SetBorder(views::CreateEmptyBorder(kImagePaddings))
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
                .SizeToFit(kLabelMaximumWidth))
        .BuildChildren();
  }

  ErrorContentsView(const ErrorContentsView&) = delete;
  ErrorContentsView& operator=(const ErrorContentsView&) = delete;
  ~ErrorContentsView() override = default;

 private:
  // MahiUiController::Observer:
  void OnStateChanged(MahiUiController::State new_state,
                      const std::optional<PayloadType>& payload) override {
    switch (new_state) {
      case MahiUiController::State::kError:
        error_status_text_->SetText(GetErrorLabelText(
            std::get<chromeos::MahiResponseStatus>(*payload)));
        return;
      case MahiUiController::State::kQuestionAndAnswer:
      case MahiUiController::State::kSummaryAndOutlines:
        return;
    }
  }

  raw_ptr<views::Label> error_status_text_ = nullptr;
};

}  // namespace

MahiErrorStatusView::MahiErrorStatusView(MahiUiController* ui_controller)
    : MahiUiController::Observer(ui_controller) {
  CHECK(chromeos::features::IsMahiEnabled());

  views::Builder<views::FlexLayoutView>(this)
      .SetBorder(views::CreateEmptyBorder(kErrorStatusViewPaddings))
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetID(mahi_constants::ViewId::kErrorStatusView)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .AddChild(views::Builder<views::View>(
          std::make_unique<ErrorContentsView>(ui_controller)))
      .BuildChildren();
}

MahiErrorStatusView::~MahiErrorStatusView() = default;

void MahiErrorStatusView::OnStateChanged(
    MahiUiController::State new_state,
    const std::optional<PayloadType>& payload) {
  switch (new_state) {
    case MahiUiController::State::kError:
      SetVisible(true);
      return;
    case MahiUiController::State::kQuestionAndAnswer:
    case MahiUiController::State::kSummaryAndOutlines:
      SetVisible(false);
      return;
  }
}

BEGIN_METADATA(MahiErrorStatusView)
END_METADATA

}  // namespace ash
