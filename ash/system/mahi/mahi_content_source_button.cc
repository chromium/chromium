// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_content_source_button.h"

#include <optional>
#include <string>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/view_factory_internal.h"

namespace ash {

namespace {

constexpr gfx::Insets kContentSourceButtonBorderInsets = gfx::Insets::VH(6, 10);
constexpr int kContentSourceImageLabelSpacing = 8;

}  // namespace

MahiContentSourceButton::MahiContentSourceButton() {
  views::Builder<views::LabelButton>(this)
      .SetCallback(
          base::BindRepeating(&MahiContentSourceButton::OpenContentSourcePage,
                              weak_ptr_factory_.GetWeakPtr()))
      .SetImageLabelSpacing(kContentSourceImageLabelSpacing)
      .SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurfaceVariant)
      .SetBorder(views::CreateEmptyBorder(kContentSourceButtonBorderInsets))
      .SetBackground(StyleUtil::CreateThemedFullyRoundedRectBackground(
          cros_tokens::kCrosSysSystemOnBase1))
      .BuildChildren();

  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation2,
                                        *label());
  RefreshContentSourceInfo();
}

MahiContentSourceButton::~MahiContentSourceButton() = default;

void MahiContentSourceButton::RefreshContentSourceInfo() {
  auto* const mahi_manager = chromeos::MahiManager::Get();
  CHECK(mahi_manager);

  content_source_url_ = mahi_manager->GetContentUrl();
  media_app_pdf_client_id_ = mahi_manager->GetMediaAppPDFClientId();
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(image_util::ResizeAndCropImage(
          mahi_manager->GetContentIcon(), mahi_constants::kContentIconSize)));
  SetText(mahi_manager->GetContentTitle());

  if (GetViewAccessibility().GetCachedName().empty()) {
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_MAHI_CONTENT_SOURCE_BUTTON_ACCESSIBLE_NAME));
  }
}

void MahiContentSourceButton::OpenContentSourcePage() {
  // If the source page is a media app PDF file, activates the media app window.
  if (media_app_pdf_client_id_ != std::nullopt) {
    if (auto* const mahi_media_app_content_manager =
            chromeos::MahiMediaAppContentManager::Get()) {
      mahi_media_app_content_manager->ActivateClientWindow(
          media_app_pdf_client_id_.value());
    } else {
      CHECK_IS_TEST();
    }
    return;
  }

  // Opens or switches to the URL.
  NewWindowDelegate::GetPrimary()->OpenUrl(
      content_source_url_, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kSwitchToTab);
}

BEGIN_METADATA(MahiContentSourceButton)
END_METADATA

}  // namespace ash
