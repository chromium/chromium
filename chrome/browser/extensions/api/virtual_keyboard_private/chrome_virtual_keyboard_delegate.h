// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_CHROME_VIRTUAL_KEYBOARD_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_CHROME_VIRTUAL_KEYBOARD_DELEGATE_H_

#include <optional>
#include <string>

#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/common/api/virtual_keyboard.h"

namespace media {
class AudioSystem;
}

namespace extensions {

class ChromeVirtualKeyboardDelegate
    : public VirtualKeyboardDelegate,
      public ash::ClipboardHistoryController::Observer {
 public:
  explicit ChromeVirtualKeyboardDelegate(
      content::BrowserContext* browser_context);

  ChromeVirtualKeyboardDelegate(const ChromeVirtualKeyboardDelegate&) = delete;
  ChromeVirtualKeyboardDelegate& operator=(
      const ChromeVirtualKeyboardDelegate&) = delete;

  ~ChromeVirtualKeyboardDelegate() override;

  // TODO(oka): Create ChromeVirtualKeyboardPrivateDelegate class and move all
  // the methods except for RestrictFeatures into the class for clear separation
  // of virtualKeyboard and virtualKeyboardPrivate API.
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
  bool SetVirtualKeyboardMode(api::virtual_keyboard_private::KeyboardMode mode,
                              gfx::Rect target_bounds,
                              OnSetModeCallback on_set_mode_callback) override;
  bool SetDraggableArea(
      const api::virtual_keyboard_private::Bounds& rect) override;
  bool SetRequestedKeyboardState(
      api::virtual_keyboard_private::KeyboardState state) override;
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

 private:
  // ash::ClipboardHistoryController::Observer:
  void OnClipboardHistoryItemsUpdated() override;

  void OnHasInputDevices(OnKeyboardSettingsCallback on_settings_callback,
                         bool has_audio_input_devices);
  void DispatchConfigChangeEvent(std::optional<base::Value::Dict> settings);

  raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  base::WeakPtr<ChromeVirtualKeyboardDelegate> weak_this_;
  base::WeakPtrFactory<ChromeVirtualKeyboardDelegate> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_VIRTUAL_KEYBOARD_PRIVATE_CHROME_VIRTUAL_KEYBOARD_DELEGATE_H_
