// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/input_method_manager.mojom-forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_bridge.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_state.h"
#include "chrome/browser/ash/arc/input_method_manager/input_connection_impl.h"
#include "chrome/browser/ash/arc/input_method_manager/input_method_prefs.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/ime/ash/ime_bridge_observer.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcInputMethodManagerService
    : public KeyedService,
      public ArcInputMethodManagerBridge::Delegate,
      public ash::input_method::InputMethodManager::ImeMenuObserver,
      public ash::input_method::InputMethodManager::Observer,
      public ash::IMEBridgeObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAndroidVirtualKeyboardVisibilityChanged(bool visible) = 0;
  };

  // The delegate class to access to the global window tree state.
  // This class separates ash dependency from ArcInputMethodManagerService.
  class WindowDelegate {
   public:
    virtual ~WindowDelegate() = default;
    virtual aura::Window* GetFocusedWindow() const = 0;
    virtual aura::Window* GetActiveWindow() const = 0;
  };

  // Returns the instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcInputMethodManagerService* GetForBrowserContext(
      content::BrowserContext* context);
  // Does the same as GetForBrowserContext() but for testing. Please refer to
  // ArcBrowserContextKeyedServiceFactoryBase for the difference between them.
  static ArcInputMethodManagerService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  static BrowserContextKeyedServiceFactory* GetFactory();

  ArcInputMethodManagerService(content::BrowserContext* context,
                               ArcBridgeService* bridge_service);

  ArcInputMethodManagerService(const ArcInputMethodManagerService&) = delete;
  ArcInputMethodManagerService& operator=(const ArcInputMethodManagerService&) =
      delete;

  ~ArcInputMethodManagerService() override;

  void SetInputMethodManagerBridgeForTesting(
      std::unique_ptr<ArcInputMethodManagerBridge> test_bridge);
  void SetWindowDelegateForTesting(std::unique_ptr<WindowDelegate> delegate);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // KeyedService overrides:
  void Shutdown() override;

  // ArcInputMethodManagerBridge::Delegate overrides:
  void OnActiveImeChanged(const std::string& ime_id) override;
  void OnImeDisabled(const std::string& ime_id) override;
  void OnImeInfoChanged(std::vector<mojom::ImeInfoPtr> ime_info_array) override;
  void OnConnectionClosed() override;

  // ash::input_method::InputMethodManager::ImeMenuObserver overrides:
  void ImeMenuListChanged() override;
  void ImeMenuActivationChanged(bool is_active) override {}
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<ash::input_method::InputMethodManager::MenuItem>& items)
      override {}

  // ash::input_method::InputMethodManager::Observer overrides:
  void InputMethodChanged(ash::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // ash::IMEBridgeObserver overrides:
  void OnInputContextHandlerChanged() override;

  // Called when a11y keyboard option changed and disables ARC IME while a11y
  // keyboard option is enabled.
  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& event_details);

  void OnArcInputMethodBoundsChanged(const gfx::Rect& bounds);

  InputConnectionImpl* GetInputConnectionForTesting();

 private:
  class ArcInputMethodBoundsObserver;
  class InputMethodEngineObserver;
  class InputMethodObserver;
  class TabletModeObserver;

  void EnableIme(const std::string& ime_id, bool enable);
  // Notify ARC's IMM of the enabled input methods in Chrome.
  void SyncEnabledImesInArc();
  // Notify ARC's IMM of the current active input method in Chrome.
  void SyncCurrentImeInArc();
  ash::input_method::InputMethodDescriptor BuildInputMethodDescriptor(
      const mojom::ImeInfo* info);
  void Focus(int input_context_id);
  void Blur();
  void UpdateTextInputState();
  mojom::TextInputStatePtr GetTextInputState(
      bool is_input_state_update_requested);

  void OnTabletModeToggled(bool enabled);

  // Update the descriptors in IMM and the prefs according to
  // |arc_ime_state_|.
  void UpdateInputMethodEntryWithImeInfo();

  // Notifies InputMethodManager's observers of possible ARC IME state changes.
  void NotifyInputMethodManagerObservers(bool is_tablet_mode);

  bool IsVirtualKeyboardShown() const;
  void SendShowVirtualKeyboard();
  void SendHideVirtualKeyboard();
  void NotifyVirtualKeyboardVisibilityChange(bool visible);

  const raw_ptr<Profile> profile_;

  std::unique_ptr<ArcInputMethodManagerBridge> imm_bridge_;
  std::set<std::string> enabled_arc_ime_ids_;
  std::unique_ptr<ArcInputMethodState::Delegate> arc_ime_state_delegate_;
  ArcInputMethodState arc_ime_state_;
  InputMethodPrefs prefs_;
  bool is_virtual_keyboard_shown_;
  // This flag is set to true while updating ARC IMEs entries in IMM to avoid
  // exposing incomplete state.
  bool is_updating_imm_entry_;

  // ArcInputMethodManager installs a proxy IME to redirect IME related events
  // from/to ARC IMEs in the container. The below two variables are for the
  // proxy IME.
  const std::string proxy_ime_extension_id_;
  std::unique_ptr<ash::input_method::InputMethodEngine> proxy_ime_engine_;

  // The current (active) input method, observed for
  // OnVirtualKeyboardVisibilityChangedIfEnabled.
  raw_ptr<ui::InputMethod> input_method_ = nullptr;
  bool is_arc_ime_active_ = false;

  std::unique_ptr<InputConnectionImpl> active_connection_;

  std::unique_ptr<TabletModeObserver> tablet_mode_observer_;

  std::unique_ptr<InputMethodObserver> input_method_observer_;

  std::unique_ptr<ArcInputMethodBoundsObserver> input_method_bounds_observer_;

  base::CallbackListSubscription accessibility_status_subscription_;

  std::unique_ptr<WindowDelegate> window_delegate_;

  base::ObserverList<Observer> observers_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_
