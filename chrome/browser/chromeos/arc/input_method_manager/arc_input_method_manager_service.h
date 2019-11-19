// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge.h"
#include "chrome/browser/chromeos/arc/input_method_manager/input_connection_impl.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "components/arc/mojom/input_method_manager.mojom.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/ime_bridge_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcInputMethodManagerService
    : public KeyedService,
      public ArcInputMethodManagerBridge::Delegate,
      public chromeos::input_method::InputMethodManager::ImeMenuObserver,
      public chromeos::input_method::InputMethodManager::Observer,
      public ui::IMEBridgeObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAndroidVirtualKeyboardVisibilityChanged(bool visible) = 0;
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
  ~ArcInputMethodManagerService() override;

  void SetInputMethodManagerBridgeForTesting(
      std::unique_ptr<ArcInputMethodManagerBridge> test_bridge);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // KeyedService overrides:
  void Shutdown() override;

  // ArcInputMethodManagerBridge::Delegate overrides:
  void OnActiveImeChanged(const std::string& ime_id) override;
  void OnImeDisabled(const std::string& ime_id) override;
  void OnImeInfoChanged(std::vector<mojom::ImeInfoPtr> ime_info_array) override;
  void OnConnectionClosed() override;

  // chromeos::input_method::InputMethodManager::ImeMenuObserver overrides:
  void ImeMenuListChanged() override;
  void ImeMenuActivationChanged(bool is_active) override {}
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<chromeos::input_method::InputMethodManager::MenuItem>&
          items) override {}

  // chromeos::input_method::InputMethodManager::Observer overrides:
  void InputMethodChanged(chromeos::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // ui::IMEBridgeObserver overrides:
  void OnRequestSwitchEngine() override {}
  void OnInputContextHandlerChanged() override;

  // Called when a11y keyboard option changed and disables ARC IME while a11y
  // keyboard option is enabled.
  void OnAccessibilityStatusChanged(
      const chromeos::AccessibilityStatusEventDetails& event_details);

  InputConnectionImpl* GetInputConnectionForTesting();

 private:
  class InputMethodEngineObserver;
  class InputMethodObserver;
  class TabletModeObserver;

  void EnableIme(const std::string& ime_id, bool enable);
  void SwitchImeTo(const std::string& ime_id);
  chromeos::input_method::InputMethodDescriptor BuildInputMethodDescriptor(
      const mojom::ImeInfo* info);
  void Focus(int input_context_id);
  void Blur();
  void UpdateTextInputState();
  mojom::TextInputStatePtr GetTextInputState(
      bool is_input_state_update_requested);

  // Removes ARC IME from IME related prefs that are current active IME pref,
  // previous active IME pref, enabled IME list pref and preloading IME list
  // pref.
  void RemoveArcIMEFromPrefs();
  void RemoveArcIMEFromPref(const char* pref_name);

  // Calls InputMethodManager.SetAllowedInputMethods according to the return
  // value of ShouldArcImeAllowed().
  void UpdateArcIMEAllowed();
  // Returns whether ARC IMEs should be allowed now or not.
  // It depends on tablet mode state and a11y keyboard option.
  bool ShouldArcIMEAllowed() const;

  // Notifies InputMethodManager's observers of possible ARC IME state changes.
  void NotifyInputMethodManagerObservers(bool is_tablet_mode);

  bool IsVirtualKeyboardShown() const;
  void SendShowVirtualKeyboard();
  void SendHideVirtualKeyboard();
  void NotifyVirtualKeyboardVisibilityChange(bool visible);

  Profile* const profile_;

  std::unique_ptr<ArcInputMethodManagerBridge> imm_bridge_;
  std::set<std::string> active_arc_ime_ids_;
  std::set<std::string> ime_ids_allowed_in_clamshell_mode_;
  bool is_virtual_keyboard_shown_;
  // This flag is set to true while updating ARC IMEs entries in IMM to avoid
  // exposing incomplete state.
  bool is_updating_imm_entry_;

  // ArcInputMethodManager installs a proxy IME to redirect IME related events
  // from/to ARC IMEs in the container. The below two variables are for the
  // proxy IME.
  const std::string proxy_ime_extension_id_;
  std::unique_ptr<chromeos::InputMethodEngine> proxy_ime_engine_;

  // The currently active input method, observed for
  // OnShowVirtualKeyboardIfEnabled.
  ui::InputMethod* input_method_ = nullptr;
  bool is_arc_ime_active_ = false;

  std::unique_ptr<InputConnectionImpl> active_connection_;

  std::unique_ptr<TabletModeObserver> tablet_mode_observer_;

  std::unique_ptr<InputMethodObserver> input_method_observer_;

  std::unique_ptr<chromeos::AccessibilityStatusSubscription>
      accessibility_status_subscription_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodManagerService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_
