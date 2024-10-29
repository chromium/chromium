// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_shortcut_hint_view.h"

#include <memory>
#include <string>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/strings/strcat.h"
#include "build/branding_buildflags.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/icons/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

constexpr int kShortcutIconSize = 16;

std::unique_ptr<views::Label> CreateShortcutTextLabel(
    const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation2,
                                        *label);
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  return label;
}

}  // namespace

PickerShortcutHintView::PickerShortcutHintView(
    QuickInsertCapsLockResult::Shortcut shortcut) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  switch (shortcut) {
    case QuickInsertCapsLockResult::Shortcut::kAltLauncher: {
      auto* alt_label = AddChildView(
          CreateShortcutTextLabel(l10n_util::GetStringUTF16(IDS_ASH_ALT_KEY)));
      auto* plus_label = AddChildView(CreateShortcutTextLabel(u" + "));
      AddChildView(
          std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
              kGdLauncherIcon, cros_tokens::kCrosSysOnSurfaceVariant,
              kShortcutIconSize)));
      shortcut_text_ = base::StrCat(
          {alt_label->GetText(), plus_label->GetText(),
           l10n_util::GetStringUTF16(IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER)});
      break;
    }
    case QuickInsertCapsLockResult::Shortcut::kAltSearch: {
      auto* alt_label = AddChildView(
          CreateShortcutTextLabel(l10n_util::GetStringUTF16(IDS_ASH_ALT_KEY)));
      auto* plus_label = AddChildView(CreateShortcutTextLabel(u" + "));
      AddChildView(
          std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
              kGdSearchIcon, cros_tokens::kCrosSysOnSurfaceVariant,
              kShortcutIconSize)));
      shortcut_text_ = base::StrCat(
          {alt_label->GetText(), plus_label->GetText(),
           l10n_util::GetStringUTF16(IDS_ASH_SHORTCUT_MODIFIER_SEARCH)});
      break;
    }
    case QuickInsertCapsLockResult::Shortcut::kFnRightAlt: {
      // TODO: b/331285414 - Shortcut hint strings and icon should be moved into
      // open source.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      auto* fn_label = AddChildView(
          CreateShortcutTextLabel(l10n_util::GetStringUTF16(IDS_ASH_FN_KEY)));
      auto* plus_label = AddChildView(CreateShortcutTextLabel(u" + "));
      AddChildView(
          std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
              kRightAltInternalIcon, cros_tokens::kCrosSysOnSurfaceVariant,
              kShortcutIconSize)));
      shortcut_text_ = base::StrCat(
          {fn_label->GetText(), plus_label->GetText(),
           l10n_util::GetStringUTF16(IDS_KEYBOARD_QUICK_INSERT_LABEL)});
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
      break;
    }
  }
}

PickerShortcutHintView::~PickerShortcutHintView() = default;

const std::u16string& PickerShortcutHintView::GetShortcutText() const {
  return shortcut_text_;
}

BEGIN_METADATA(PickerShortcutHintView)
END_METADATA

}  // namespace ash
