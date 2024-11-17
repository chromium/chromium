// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_strings.h"

#include <string>

#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/views/quick_insert_category_type.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

std::u16string GetLabelForQuickInsertCategory(QuickInsertCategory category) {
  switch (category) {
    case QuickInsertCategory::kEditorWrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_WRITE_CARD_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case QuickInsertCategory::kEditorRewrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(IDS_EDITOR_MENU_REWRITE_CARD_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case QuickInsertCategory::kLobsterWithNoSelectedText:
    case QuickInsertCategory::kLobsterWithSelectedText:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(IDS_PICKER_LOBSTER_SELECTION_LABEL);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case QuickInsertCategory::kLinks:
      return l10n_util::GetStringUTF16(IDS_PICKER_LINKS_CATEGORY_LABEL);
    case QuickInsertCategory::kEmojisGifs:
      return l10n_util::GetStringUTF16(IDS_PICKER_EXPRESSIONS_CATEGORY_LABEL);
    case QuickInsertCategory::kEmojis:
      return l10n_util::GetStringUTF16(IDS_PICKER_EMOJIS_CATEGORY_LABEL);
    case QuickInsertCategory::kClipboard:
      return l10n_util::GetStringUTF16(IDS_PICKER_CLIPBOARD_CATEGORY_LABEL);
    case QuickInsertCategory::kDriveFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_DRIVE_FILES_CATEGORY_LABEL);
    case QuickInsertCategory::kLocalFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_LOCAL_FILES_CATEGORY_LABEL);
    case QuickInsertCategory::kDatesTimes:
      return l10n_util::GetStringUTF16(IDS_PICKER_DATES_TIMES_CATEGORY_LABEL);
    case QuickInsertCategory::kUnitsMaths:
      return l10n_util::GetStringUTF16(IDS_PICKER_UNITS_MATHS_CATEGORY_LABEL);
  }
}

std::u16string GetSearchFieldPlaceholderTextForQuickInsertCategory(
    QuickInsertCategory category) {
  switch (category) {
    case QuickInsertCategory::kLinks:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_LINKS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case QuickInsertCategory::kClipboard:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_CLIPBOARD_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case QuickInsertCategory::kDriveFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_DRIVE_FILES_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case QuickInsertCategory::kLocalFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_LOCAL_FILES_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case QuickInsertCategory::kDatesTimes:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_DATES_TIMES_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case QuickInsertCategory::kUnitsMaths:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_UNITS_MATHS_CATEGORY_SEARCH_FIELD_PLACEHOLDER_TEXT);
    case QuickInsertCategory::kEditorWrite:
    case QuickInsertCategory::kEditorRewrite:
    case QuickInsertCategory::kLobsterWithNoSelectedText:
    case QuickInsertCategory::kLobsterWithSelectedText:
    case QuickInsertCategory::kEmojisGifs:
    case QuickInsertCategory::kEmojis:
      NOTREACHED();
  }
}

std::u16string GetSectionTitleForQuickInsertCategoryType(
    QuickInsertCategoryType category_type) {
  switch (category_type) {
    case QuickInsertCategoryType::kEditorWrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDITOR_WRITE_CATEGORY_TYPE_SECTION_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case QuickInsertCategoryType::kEditorRewrite:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDITOR_REWRITE_CATEGORY_TYPE_SECTION_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case QuickInsertCategoryType::kLobster:
      return u"";
    case QuickInsertCategoryType::kGeneral:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_GENERAL_CATEGORY_TYPE_SECTION_TITLE);
    case QuickInsertCategoryType::kMore:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_MORE_CATEGORY_TYPE_SECTION_TITLE);
    case QuickInsertCategoryType::kCaseTransformations:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDIT_TEXT_CATEGORY_TYPE_SECTION_TITLE);
    case QuickInsertCategoryType::kNone:
      return u"";
  }
}

std::u16string GetSectionTitleForQuickInsertSectionType(
    QuickInsertSectionType section_type) {
  switch (section_type) {
    case QuickInsertSectionType::kNone:
      return u"";
    case QuickInsertSectionType::kClipboard:
      return l10n_util::GetStringUTF16(IDS_PICKER_CLIPBOARD_CATEGORY_LABEL);
    case QuickInsertSectionType::kExamples:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EXAMPLES_CATEGORY_TYPE_SECTION_TITLE);
    case QuickInsertSectionType::kContentEditor:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // TODO: b/369726248 - Rename the IDS variable name to a generic name.
      return l10n_util::GetStringUTF16(
          IDS_PICKER_EDITOR_WRITE_CATEGORY_TYPE_SECTION_TITLE);
#else
      return u"";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case QuickInsertSectionType::kLinks:
      return l10n_util::GetStringUTF16(IDS_PICKER_LINKS_CATEGORY_LABEL);
    case QuickInsertSectionType::kLocalFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_LOCAL_FILES_CATEGORY_LABEL);
    case QuickInsertSectionType::kDriveFiles:
      return l10n_util::GetStringUTF16(IDS_PICKER_DRIVE_FILES_CATEGORY_LABEL);
  }
}

}  // namespace ash
