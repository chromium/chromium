// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_

#include <optional>
#include <utility>

#include "ash/public/cpp/input_device_settings_controller.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/wm/public/activation_client.h"

class PrefService;

namespace ash {

class DeprecationNotificationController;
class InputDeviceSettingsNotificationController;

class EventRewriterDelegateImpl : public ui::EventRewriterAsh::Delegate {
 public:
  explicit EventRewriterDelegateImpl(wm::ActivationClient* activation_client);
  EventRewriterDelegateImpl(
      wm::ActivationClient* activation_client,
      std::unique_ptr<DeprecationNotificationController> deprecation_controller,
      std::unique_ptr<InputDeviceSettingsNotificationController>
          input_device_settings_notification_controller,
      InputDeviceSettingsController* input_device_settings_controller);

  EventRewriterDelegateImpl(const EventRewriterDelegateImpl&) = delete;
  EventRewriterDelegateImpl& operator=(const EventRewriterDelegateImpl&) =
      delete;

  ~EventRewriterDelegateImpl() override;

  void set_pref_service_for_testing(PrefService* pref_service) {
    pref_service_for_testing_ = pref_service;
  }

  // ui::EventRewriterAsh::Delegate:
  bool RewriteModifierKeys() override;
  bool RewriteMetaTopRowKeyComboEvents(int device_id) const override;
  std::optional<ui::mojom::ModifierKey> GetKeyboardRemappedModifierValue(
      int device_id,
      ui::mojom::ModifierKey modifier_key,
      const std::string& pref_name) const override;
  bool TopRowKeysAreFunctionKeys(int device_id) const override;
  bool IsExtensionCommandRegistered(ui::KeyboardCode key_code,
                                    int flags) const override;
  bool IsSearchKeyAcceleratorReserved() const override;
  bool NotifyDeprecatedRightClickRewrite() override;
  bool NotifyDeprecatedSixPackKeyRewrite(ui::KeyboardCode key_code) override;
  void SuppressModifierKeyRewrites(bool should_suppress) override;
  void SuppressMetaTopRowKeyComboRewrites(bool should_suppress) override;
  void RecordEventRemappedToRightClick(bool alt_based_right_click) override;
  void RecordSixPackEventRewrite(ui::KeyboardCode key_code,
                                 bool alt_based) override;
  std::optional<ui::mojom::SimulateRightClickModifier>
  GetRemapRightClickModifier(int device_id) override;
  std::optional<ui::mojom::SixPackShortcutModifier>
  GetShortcutModifierForSixPackKey(int device_id,
                                   ui::KeyboardCode key_code) override;
  void NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier blocked_modifier,
      ui::mojom::SimulateRightClickModifier active_modifier) override;

  void NotifySixPackRewriteBlockedBySetting(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier blocked_modifier,
      ui::mojom::SixPackShortcutModifier active_modifier,
      int device_id) override;

  std::optional<ui::mojom::ExtendedFkeysModifier> GetExtendedFkeySetting(
      int device_id,
      ui::KeyboardCode key_code) override;

  // Push a notification when the device has an Fn key and six pack shortcut
  // with search key is pressed.
  void NotifySixPackRewriteBlockedByFnKey(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier modifier) override;

  // Push a notification when the device has an Fn key and search plus top
  // row key is pressed.
  void NotifyTopRowRewriteBlockedByFnKey() override;

  // Workaround for test behavior injection.
  // Currently, there's no easier way to inject
  // ExtensionCommandsGlobalRegistry's behavior, so difficult to test
  // EventRewriterAsh with changing IsExtensionCommandRegistered's behavior.
  // Introducing a fake class of Delegate implementation can solve the issue,
  // but without further larger refactoring some testing coverage will be lost.
  // This API is for the mitigation for short term.
  // TODO(crbug.com/40265877): Consider clear separation of EventRewriterAsh's
  // test and Delegate's test, then to remove this API.
  void SetExtensionCommandsOverrideForTesting(
      std::optional<base::flat_set<std::pair<ui::KeyboardCode, int>>>
          commands) {
    extension_commands_override_for_testing_ = std::move(commands);
  }

 private:
  PrefService* GetPrefService() const;

  raw_ptr<PrefService> pref_service_for_testing_;

  std::optional<base::flat_set<std::pair<ui::KeyboardCode, int>>>
      extension_commands_override_for_testing_;

  raw_ptr<wm::ActivationClient, DanglingUntriaged> activation_client_;

  // Handles showing notifications when deprecated event rewrites occur.
  std::unique_ptr<DeprecationNotificationController> deprecation_controller_;
  std::unique_ptr<InputDeviceSettingsNotificationController>
      input_device_settings_notification_controller_;

  // Tracks whether modifier rewrites should be suppressed or not.
  bool suppress_modifier_key_rewrites_ = false;

  // Tracks whether meta + top row key rewrites should be suppressed or not.
  bool suppress_meta_top_row_key_rewrites_ = false;

  raw_ptr<InputDeviceSettingsController, DanglingUntriaged>
      input_device_settings_controller_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EVENTS_EVENT_REWRITER_DELEGATE_IMPL_H_
