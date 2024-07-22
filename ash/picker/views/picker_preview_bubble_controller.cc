// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_preview_bubble_controller.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/picker/views/picker_preview_bubble.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// TODO: b/322899031 - Translate this string.
constexpr std::u16string_view kEyebrowText = u"Last action";

// Duration to wait before showing the preview bubble when it is requested.
constexpr base::TimeDelta kShowBubbleDelay = base::Milliseconds(600);

// Taken from //chrome/browser/ash/app_list/search/files/justifications.cc.
// Time limits for how last accessed or modified time maps to each justification
// string.
constexpr base::TimeDelta kJustNow = base::Minutes(15);

std::u16string GetTimeString(base::Time timestamp) {
  const base::Time now = base::Time::Now();
  const base::Time midnight = now.LocalMidnight();
  if ((now - timestamp).magnitude() <= kJustNow) {
    return l10n_util::GetStringUTF16(
        IDS_FILE_SUGGESTION_JUSTIFICATION_TIME_NOW);
  }

  if (timestamp >= midnight && timestamp < midnight + base::Days(1)) {
    return base::TimeFormatTimeOfDay(timestamp);
  }

  return base::LocalizedTimeFormatWithPattern(timestamp, "MMMd");
}

std::u16string GetJustificationString(base::Time viewed, base::Time modified) {
  // Prefer "modified" over "viewed" if they are the same.
  if (modified >= viewed) {
    return l10n_util::GetStringFUTF16(
        IDS_FILE_SUGGESTION_JUSTIFICATION,
        l10n_util::GetStringUTF16(
            IDS_FILE_SUGGESTION_JUSTIFICATION_GENERIC_MODIFIED_ACTION),
        GetTimeString(modified));
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_FILE_SUGGESTION_JUSTIFICATION,
        l10n_util::GetStringUTF16(
            IDS_FILE_SUGGESTION_JUSTIFICATION_YOU_VIEWED_ACTION),
        GetTimeString(viewed));
  }
}

}  // namespace

PickerPreviewBubbleController::PickerPreviewBubbleController() = default;

PickerPreviewBubbleController::~PickerPreviewBubbleController() {
  CloseBubble();
}

void PickerPreviewBubbleController::ShowBubbleAfterDelay(
    HoldingSpaceImage* async_preview_image,
    const base::FilePath& path,
    views::View* anchor_view) {
  CreateBubbleWidget(
      async_preview_image,
      base::BindOnce(
          [](base::FilePath path) -> std::optional<base::File::Info> {
            base::File::Info info;
            if (!base::GetFileInfo(path, &info)) {
              return std::nullopt;
            }
            return info;
          },
          path),
      anchor_view);
  show_bubble_timer_.Start(
      FROM_HERE, kShowBubbleDelay,
      base::BindOnce(&PickerPreviewBubbleController::ShowBubble,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PickerPreviewBubbleController::CloseBubble() {
  if (bubble_view_ == nullptr) {
    return;
  }
  bubble_view_->Close();
  OnWidgetDestroying(bubble_view_->GetWidget());
}

void PickerPreviewBubbleController::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  bubble_view_ = nullptr;

  async_preview_image_ = nullptr;
}

void PickerPreviewBubbleController::ShowBubbleImmediatelyForTesting(
    HoldingSpaceImage* async_preview_image,
    base::OnceCallback<std::optional<base::File::Info>()> get_file_info,
    views::View* anchor_view) {
  CreateBubbleWidget(async_preview_image, std::move(get_file_info),
                     anchor_view);
  bubble_view_->GetWidget()->Show();
}

PickerPreviewBubbleView*
PickerPreviewBubbleController::bubble_view_for_testing() const {
  return bubble_view_;
}

void PickerPreviewBubbleController::UpdateBubbleImage() {
  if (bubble_view_ != nullptr) {
    bubble_view_->SetPreviewImage(
        ui::ImageModel::FromImageSkia(async_preview_image_->GetImageSkia(
            PickerPreviewBubbleView::kPreviewImageSize)));
  }
}

void PickerPreviewBubbleController::UpdateBubbleMetadata(
    std::optional<base::File::Info> info) {
  if (bubble_view_ == nullptr) {
    return;
  }
  if (!info.has_value()) {
    return;
  }
  if (info->last_modified.is_null() && info->last_accessed.is_null()) {
    return;
  }

  bubble_view_->SetText(
      std::u16string(kEyebrowText),
      GetJustificationString(info->last_accessed, info->last_modified));
}

void PickerPreviewBubbleController::CreateBubbleWidget(
    HoldingSpaceImage* async_preview_image,
    base::OnceCallback<std::optional<base::File::Info>()> get_file_info,
    views::View* anchor_view) {
  if (bubble_view_ != nullptr) {
    return;
  }

  CHECK(anchor_view);
  bubble_view_ = new PickerPreviewBubbleView(anchor_view);
  async_preview_image_ = async_preview_image;
  bubble_view_->SetPreviewImage(
      ui::ImageModel::FromImageSkia(async_preview_image_->GetImageSkia()));
  // base::Unretained is safe here since `image_subscription_` is a member.
  // During destruction, `image_subscription_` will be destroyed before the
  // other members, so the callback is guaranteed to be safe.
  image_subscription_ = async_preview_image_->AddImageSkiaChangedCallback(
      base::BindRepeating(&PickerPreviewBubbleController::UpdateBubbleImage,
                          base::Unretained(this)));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      std::move(get_file_info),
      base::BindOnce(&PickerPreviewBubbleController::UpdateBubbleMetadata,
                     weak_ptr_factory_.GetWeakPtr()));

  widget_observation_.Observe(bubble_view_->GetWidget());
}

void PickerPreviewBubbleController::ShowBubble() {
  if (bubble_view_ != nullptr) {
    bubble_view_->GetWidget()->Show();
  }
}

}  // namespace ash
