// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_strings.h"

#include <string>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

std::u16string GetLabelForPickerCategory(PickerCategory category) {
  switch (category) {
    case PickerCategory::kEditorWrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_WRITE_CARD_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case PickerCategory::kEditorRewrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_REWRITE_CARD_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case PickerCategory::kLobster:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(IDS_PICKER_LOBSTER_SELECTION_LABEL);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case PickerCategory::kLinks:
      return l10n_util::GetStringUTF16(IDS_PICKER_LINKS_CATEGORY_LABEL);
    case PickerCategory::kEmojisGifs:
      return l10n_util::GetStringUTF16(IDS_PICKER_EXPRESSIONS_CATEGORY_LABEL);
    case PickerCategory::kEmojis:
      return l10n_util::GetStringUTF16(IDS_PICKER_EMOJIS_CATEGORY_LABEL);
    case PickerCategory::kClipboard:
      return l10n_util::GetStringUTF16(IDS_PICKER_CLIPBOARD_CATEGORY_LABEL);
    case PickerCategory::kDriveFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_DRIVE_FILES_CATEGORY_LABEL);
    case PickerCategory::kLocalFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_LOCAL_FILES_CATEGORY_LABEL);
    case PickerCategory::kDatesTimes:
      return l10n_util::GetStringUTF16(IDS_PICKER_DATES_TIMES_CATEGORY_LABEL);
    case PickerCategory::kUnitsMaths:
      return l10n_util::GetStringUTF16(IDS_PICKER_UNITS_MATHS_CATEGORY_LABEL);
  }
}

std::u16string GetSearchFieldPlaceholderTextForPickerCategory(
    PickerCategory category) {
  switch (category) {
    case PickerCategory::kLinks:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_LINKS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kClipboard:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_CLIPBOARD_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kDriveFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_DRIVE_FILES_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kLocalFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_LOCAL_FILES_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kDatesTimes:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_DATES_TIMES_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kUnitsMaths:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_UNITS_MATHS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case PickerCategory::kEditorWrite:
    case PickerCategory::kEditorRewrite:
    case PickerCategory::kLobster:
    case PickerCategory::kEmojisGifs:
    case PickerCategory::kEmojis:
      NOTREACHED_NORETURN();
  }
}

std::u16string GetSectionTitleForPickerCategoryType(
    PickerCategoryType category_type) {
  switch (category_type) {
    case PickerCategoryType::kEditorWrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDITOR_WRITE_CATEGORY_TYPE_SECTION_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case PickerCategoryType::kEditorRewrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDITOR_REWRITE_CATEGORY_TYPE_SECTION_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case PickerCategoryType::kLobster:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDITOR_WRITE_CATEGORY_TYPE_SECTION_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

    case PickerCategoryType::kGeneral:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_GENERAL_CATEGORY_TYPE_SECTION_TITLE);
    case PickerCategoryType::kMore:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_MORE_CATEGORY_TYPE_SECTION_TITLE);
    case PickerCategoryType::kCaseTransformations:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDIT_TEXT_CATEGORY_TYPE_SECTION_TITLE);
    case PickerCategoryType::kNone:
      return u"";
  }
}

std::u16string GetSectionTitleForPickerSectionType(
    PickerSectionType section_type) {
  switch (section_type) {
    case PickerSectionType::kNone:
      return u"";
    case PickerSectionType::kClipboard:
      return l10n_util::GetStringUTF16(IDS_PICKER_CLIPBOARD_CATEGORY_LABEL);
    case PickerSectionType::kExamples:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EXAMPLES_CATEGORY_TYPE_SECTION_TITLE);
    case PickerSectionType::kContentEditor:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // TODO: b/369726248 - Rename the IDS variable name to a generic name.
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDITOR_WRITE_CATEGORY_TYPE_SECTION_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case PickerSectionType::kLinks:
      return l10n_util::GetStringUTF16(IDS_PICKER_LINKS_CATEGORY_LABEL);
    case PickerSectionType::kLocalFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_LOCAL_FILES_CATEGORY_LABEL);
    case PickerSectionType::kDriveFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_DRIVE_FILES_CATEGORY_LABEL);
  }
}

}  // namespace ash
