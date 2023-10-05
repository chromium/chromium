// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_

#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/input_method/editor_client_connector.h"
#include "chrome/browser/ash/input_method/editor_consent_store.h"
#include "chrome/browser/ash/input_method/editor_event_proxy.h"
#include "chrome/browser/ash/input_method/editor_event_sink.h"
#include "chrome/browser/ash/input_method/editor_instance_impl.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/input_method/editor_service_connector.h"
#include "chrome/browser/ash/input_method/editor_switch.h"
#include "chrome/browser/ash/input_method/editor_text_actuator.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/ash/input_method/mojom/editor.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace input_method {

// Acts as a central "connector" for all things related to the orca project.
// This includes all current (and future) trigger points, providing the required
// plumbing to broker mojo connections from WebUIs and other clients, and
// providing an overall unified interface for the backend of the project.

class EditorMediator
    : public EditorInstanceImpl::Delegate,
      public EditorEventSink,
      public ProfileObserver,
      public EditorPanelManager::Delegate, 
      public EditorTextActuator::Delegate,
      public TabletModeObserver,
      public user_manager::UserManager::UserSessionStateObserver {

 public:
  // country_code that determines the country/territory in which the device is
  // situated.
  EditorMediator(Profile* profile, std::string_view country_code);
  ~EditorMediator() override;

  // Fetch the current instance of this class. Note that this class MUST be
  // constructed prior to calling this method.
  static EditorMediator* Get();

  static bool HasInstance();

  // Binds a new editor instance request from a client.
  void BindEditorClient(mojo::PendingReceiver<orca::mojom::EditorClient>
                            pending_receiver) override;

  // Binds a new panel manager request from a client.
  void BindEditorPanelManager(
      mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
          pending_receiver);

  // EditorEventSink
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnActivateIme(std::string_view engine_id) override;
  void OnSurroundingTextChanged(const std::u16string& text,
                                gfx::Range selection_range) override;

  // EditorPanelManager::Delegate
  void OnPromoCardDeclined() override;
  // TODO(b/301869966): Consider removing default parameters once the context
  // menu Orca entry is removed.
  void HandleTrigger(
      absl::optional<std::string_view> preset_query_id = absl::nullopt,
      absl::optional<std::string_view> freeform_text = absl::nullopt) override;
  EditorMode GetEditorMode() const override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // EditorTextActuator::Delegate overrides
  void OnTextInserted() override;
  void ProcessConsentAction(ConsentAction consent_action) override;

  // Checks if the feature should be visible.
  bool IsAllowedForUse();

  // ProfileObserver overrides:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  void ActiveUserChanged(user_manager::User* user) override;

  void SetProfileByUser(user_manager::User* user);

  EditorPanelManager& panel_manager() { return panel_manager_; }

 private:
  void OnTextFieldContextualInfoChanged(const TextFieldContextualInfo& info);

  void SetUpNewEditorService();
  void BindEditor();
  void OnEditorServiceConnected(bool is_connection_bound);

  bool GetUserPref();
  void SetUserPref(bool value);

  // Not owned by this class
  raw_ptr<Profile> profile_;

  EditorInstanceImpl editor_instance_impl_;
  EditorPanelManager panel_manager_;
  MakoBubbleCoordinator mako_bubble_coordinator_;

  std::unique_ptr<EditorSwitch> editor_switch_;
  std::unique_ptr<EditorConsentStore> consent_store_;
  EditorServiceConnector editor_service_connector_;

  // TODO: b:298285960 - add the instantiation of this instance.
  std::unique_ptr<EditorEventProxy> editor_event_proxy_;
  std::unique_ptr<EditorClientConnector> editor_client_connector_;
  std::unique_ptr<EditorTextQueryProvider> text_query_provider_;
  std::unique_ptr<EditorTextActuator> text_actuator_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::ScopedObservation<TabletMode, TabletModeObserver>
      tablet_mode_observation_{this};

  base::WeakPtrFactory<EditorMediator> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
