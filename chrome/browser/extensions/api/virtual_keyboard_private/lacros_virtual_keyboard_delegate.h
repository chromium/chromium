// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_LACROS_VIRTUAL_KEYBOARD_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_LACROS_VIRTUAL_KEYBOARD_DELEGATE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/virtual_keyboard.mojom.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/common/api/virtual_keyboard.h"

namespace extensions {

// The virtual keyboard api delegate for lacros browser, it handles virtual
// keyboar api request from lacros browser extensions. Currently it only
// supports RestrictFeatures requests, all other apis are unimplemented.
class LacrosVirtualKeyboardDelegate : public VirtualKeyboardDelegate {
 public:
  LacrosVirtualKeyboardDelegate();

  LacrosVirtualKeyboardDelegate(const LacrosVirtualKeyboardDelegate&) = delete;
  LacrosVirtualKeyboardDelegate& operator=(
      const LacrosVirtualKeyboardDelegate&) = delete;

  ~LacrosVirtualKeyboardDelegate() override;

 private:
  // VirtualKeyboardDelegate impl:
  void GetKeyboardConfig(
      OnKeyboardSettingsCallback on_settings_callback) override;
  void OnKeyboardConfigChanged() override;
  bool HideKeyboard() override;
  bool InsertText(const std::u16string& text) override;
  bool OnKeyboardLoaded() override;
  void SetHotrodKeyboard(bool enable) override;
  bool LockKeyboard(bool state) override;
  bool SendKeyEvent(const std::string& type,
                    int char_value,
                    int key_code,
                    const std::string& key_name,
                    int modifiers) override;
  bool ShowLanguageSettings() override;
  bool ShowSuggestionSettings() override;
  bool IsSettingsEnabled() override;
  bool SetVirtualKeyboardMode(int mode_enum,
                              gfx::Rect target_bounds,
                              OnSetModeCallback on_set_mode_callback) override;
  bool SetDraggableArea(
      const api::virtual_keyboard_private::Bounds& rect) override;
  bool SetRequestedKeyboardState(int state_enum) override;
  bool SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override;
  bool SetAreaToRemainOnScreen(const gfx::Rect& bounds) override;
  bool SetWindowBoundsInScreen(const gfx::Rect& bounds_in_screen) override;
  void GetClipboardHistory(
      OnGetClipboardHistoryCallback get_history_callback) override;
  bool PasteClipboardItem(const std::string& clipboard_item_id) override;
  bool DeleteClipboardItem(const std::string& clipboard_item_id) override;
  void RestrictFeatures(
      const api::virtual_keyboard::RestrictFeatures::Params& params,
      OnRestrictFeaturesCallback callback) override;

  void ParseRestrictFeaturesResult(
      OnRestrictFeaturesCallback callback,
      crosapi::mojom::VirtualKeyboardRestrictionsPtr update);

  base::WeakPtrFactory<LacrosVirtualKeyboardDelegate> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_LACROS_VIRTUAL_KEYBOARD_DELEGATE_H_
