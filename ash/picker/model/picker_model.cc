// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/model/picker_model.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/picker/model/picker_mode_type.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "base/check_deref.h"
#include "chromeos/components/editor_menu/public/cpp/editor_helpers.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/range/range.h"

namespace ash {
namespace {

std::u16string GetSelectedText(ui::TextInputClient* client) {
  gfx::Range selection_range;
  std::u16string result;
  if (client && client->GetEditableSelectionRange(&selection_range) &&
      selection_range.IsValid() && !selection_range.is_empty() &&
      client->GetTextFromRange(selection_range, &result)) {
    return result;
  }
  return u"";
}

gfx::Range GetSelectionRange(ui::TextInputClient* client) {
  gfx::Range selection_range;
  return client && client->GetEditableSelectionRange(&selection_range)
             ? selection_range
             : gfx::Range();
}

ui::TextInputType GetTextInputType(ui::TextInputClient* client) {
  return client ? client->GetTextInputType()
                : ui::TextInputType::TEXT_INPUT_TYPE_NONE;
}

}  // namespace

PickerModel::PickerModel(ui::TextInputClient* focused_client,
                         input_method::ImeKeyboard* ime_keyboard,
                         EditorStatus editor_status)
    : has_focus_(focused_client != nullptr &&
                 focused_client->GetTextInputType() !=
                     ui::TextInputType::TEXT_INPUT_TYPE_NONE),
      selected_text_(GetSelectedText(focused_client)),
      selection_range_(GetSelectionRange(focused_client)),
      is_caps_lock_enabled_(CHECK_DEREF(ime_keyboard).IsCapsLockEnabled()),
      editor_status_(editor_status),
      text_input_type_(GetTextInputType(focused_client)) {}

std::vector<PickerCategory> PickerModel::GetAvailableCategories() const {
  switch (GetMode()) {
    case PickerModeType::kUnfocused:
      return std::vector<PickerCategory>{
          PickerCategory::kLinks,
          PickerCategory::kDriveFiles,
          PickerCategory::kLocalFiles,
      };
    case PickerModeType::kHasSelection: {
      std::vector<PickerCategory> categories;
      if (editor_status_ == EditorStatus::kEnabled) {
        categories.push_back(PickerCategory::kEditorRewrite);
      }
      return categories;
    }
    case PickerModeType::kNoSelection: {
      std::vector<PickerCategory> categories;
      if (editor_status_ == EditorStatus::kEnabled) {
        categories.push_back(PickerCategory::kEditorWrite);
      }
      categories.push_back(PickerCategory::kLinks);
      if (text_input_type_ != ui::TextInputType::TEXT_INPUT_TYPE_URL) {
        categories.push_back(PickerCategory::kExpressions);
      }
      categories.insert(categories.end(), {
                                              PickerCategory::kClipboard,
                                              PickerCategory::kDriveFiles,
                                              PickerCategory::kLocalFiles,
                                              PickerCategory::kDatesTimes,
                                              PickerCategory::kUnitsMaths,
                                          });

      return categories;
    }
    case PickerModeType::kPassword: {
      return {};
    }
  }
}

std::vector<PickerCategory> PickerModel::GetRecentResultsCategories() const {
  if (GetMode() == PickerModeType::kHasSelection) {
    return std::vector<PickerCategory>{};
  }

  return {
      PickerCategory::kDriveFiles,
      PickerCategory::kLocalFiles,
      PickerCategory::kLinks,
  };
}

std::u16string_view PickerModel::selected_text() const {
  return selected_text_;
}

bool PickerModel::is_caps_lock_enabled() const {
  return is_caps_lock_enabled_;
}

PickerModeType PickerModel::GetMode() const {
  if (!has_focus_) {
    return PickerModeType::kUnfocused;
  }

  if (text_input_type_ == ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD) {
    return PickerModeType::kPassword;
  }

  return chromeos::editor_helpers::NonWhitespaceAndSymbolsLength(
             selected_text_, gfx::Range(0, selection_range_.end() -
                                               selection_range_.start())) == 0
             ? PickerModeType::kNoSelection
             : PickerModeType::kHasSelection;
}

bool PickerModel::IsGifsEnabled(PrefService* prefs) const {
  if (const PrefService::Preference* pref =
          prefs->FindPreference(prefs::kEmojiPickerGifSupportEnabled)) {
    return pref->GetValue()->GetBool();
  }
  return false;
}

}  // namespace ash
