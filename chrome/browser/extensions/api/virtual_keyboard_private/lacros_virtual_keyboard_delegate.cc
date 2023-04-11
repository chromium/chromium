// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/virtual_keyboard_private/lacros_virtual_keyboard_delegate.h"

#include "base/notreached.h"
#include "chromeos/crosapi/mojom/virtual_keyboard.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace extensions {
namespace {
void UpdateRestriction(
    crosapi::mojom::VirtualKeyboardFeature feature,
    bool enabled,
    crosapi::mojom::VirtualKeyboardRestrictions* restrictions) {
  if (enabled) {
    restrictions->enabled_features.push_back(feature);
  } else {
    restrictions->disabled_features.push_back(feature);
  }
}

void PopulateFeatureRestrictions(
    const std::vector<crosapi::mojom::VirtualKeyboardFeature>& features,
    bool enabled,
    api::virtual_keyboard::FeatureRestrictions* update) {
  for (auto feature : features) {
    switch (feature) {
      case crosapi::mojom::VirtualKeyboardFeature::AUTOCOMPLETE:
        update->auto_complete_enabled = enabled;
        break;
      case crosapi::mojom::VirtualKeyboardFeature::AUTOCORRECT:
        update->auto_correct_enabled = enabled;
        break;
      case crosapi::mojom::VirtualKeyboardFeature::HANDWRITING:
        update->handwriting_enabled = enabled;
        break;
      case crosapi::mojom::VirtualKeyboardFeature::SPELL_CHECK:
        update->spell_check_enabled = enabled;
        break;
      case crosapi::mojom::VirtualKeyboardFeature::VOICE_INPUT:
        update->voice_input_enabled = enabled;
        break;
      case crosapi::mojom::VirtualKeyboardFeature::NONE:
        NOTREACHED();
        break;
    }
  }
}
}  // namespace

LacrosVirtualKeyboardDelegate::LacrosVirtualKeyboardDelegate() = default;

LacrosVirtualKeyboardDelegate::~LacrosVirtualKeyboardDelegate() = default;

void LacrosVirtualKeyboardDelegate::GetKeyboardConfig(
    OnKeyboardSettingsCallback on_settings_callback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void LacrosVirtualKeyboardDelegate::OnKeyboardConfigChanged() {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool LacrosVirtualKeyboardDelegate::HideKeyboard() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::InsertText(const std::u16string& text) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::OnKeyboardLoaded() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void LacrosVirtualKeyboardDelegate::SetHotrodKeyboard(bool enable) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool LacrosVirtualKeyboardDelegate::LockKeyboard(bool state) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SendKeyEvent(const std::string& type,
                                                 int char_value,
                                                 int key_code,
                                                 const std::string& key_name,
                                                 int modifiers) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::ShowLanguageSettings() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::ShowSuggestionSettings() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::IsSettingsEnabled() {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SetVirtualKeyboardMode(
    api::virtual_keyboard_private::KeyboardMode mode,
    gfx::Rect target_bounds,
    OnSetModeCallback on_set_mode_callback) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SetDraggableArea(
    const api::virtual_keyboard_private::Bounds& rect) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SetRequestedKeyboardState(
    api::virtual_keyboard_private::KeyboardState state) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SetOccludedBounds(
    const std::vector<gfx::Rect>& bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SetHitTestBounds(
    const std::vector<gfx::Rect>& bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SetAreaToRemainOnScreen(
    const gfx::Rect& bounds) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::SetWindowBoundsInScreen(
    const gfx::Rect& bounds_in_screen) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void LacrosVirtualKeyboardDelegate::GetClipboardHistory(
    OnGetClipboardHistoryCallback get_history_callback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool LacrosVirtualKeyboardDelegate::PasteClipboardItem(
    const std::string& clipboard_item_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool LacrosVirtualKeyboardDelegate::DeleteClipboardItem(
    const std::string& clipboard_item_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void LacrosVirtualKeyboardDelegate::ParseRestrictFeaturesResult(
    OnRestrictFeaturesCallback callback,
    crosapi::mojom::VirtualKeyboardRestrictionsPtr update_mojo) {
  api::virtual_keyboard::FeatureRestrictions update;
  PopulateFeatureRestrictions(update_mojo->enabled_features, true, &update);
  PopulateFeatureRestrictions(update_mojo->disabled_features, false, &update);
  std::move(callback).Run(std::move(update));
}

void LacrosVirtualKeyboardDelegate::RestrictFeatures(
    const api::virtual_keyboard::RestrictFeatures::Params& params,
    OnRestrictFeaturesCallback callback) {
  const api::virtual_keyboard::FeatureRestrictions& restrictions =
      params.restrictions;

  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::VirtualKeyboard>()) {
    std::move(callback).Run(api::virtual_keyboard::FeatureRestrictions());
    return;
  }

  auto restrictions_mojo = crosapi::mojom::VirtualKeyboardRestrictions::New();
  if (restrictions.auto_complete_enabled) {
    UpdateRestriction(crosapi::mojom::VirtualKeyboardFeature::AUTOCOMPLETE,
                      *restrictions.auto_complete_enabled,
                      restrictions_mojo.get());
  }
  if (restrictions.auto_correct_enabled) {
    UpdateRestriction(crosapi::mojom::VirtualKeyboardFeature::AUTOCORRECT,
                      *restrictions.auto_correct_enabled,
                      restrictions_mojo.get());
  }
  if (restrictions.handwriting_enabled) {
    UpdateRestriction(crosapi::mojom::VirtualKeyboardFeature::HANDWRITING,
                      *restrictions.handwriting_enabled,
                      restrictions_mojo.get());
  }
  if (restrictions.spell_check_enabled) {
    UpdateRestriction(crosapi::mojom::VirtualKeyboardFeature::SPELL_CHECK,
                      *restrictions.spell_check_enabled,
                      restrictions_mojo.get());
  }
  if (restrictions.voice_input_enabled) {
    UpdateRestriction(crosapi::mojom::VirtualKeyboardFeature::VOICE_INPUT,
                      *restrictions.voice_input_enabled,
                      restrictions_mojo.get());
  }

  lacros_service->GetRemote<crosapi::mojom::VirtualKeyboard>()
      ->RestrictFeatures(
          std::move(restrictions_mojo),
          base::BindOnce(
              &LacrosVirtualKeyboardDelegate::ParseRestrictFeaturesResult,
              weak_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace extensions
