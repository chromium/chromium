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
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/input_method/editor_service_connector.h"
#include "chrome/browser/ash/input_method/editor_switch.h"
#include "chrome/browser/ash/input_method/editor_text_actuator.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {
namespace input_method {

// Acts as a central "connector" for all things related to the orca project.
// This includes all current (and future) trigger points, providing the required
// plumbing to broker mojo connections from WebUIs and other clients, and
// providing an overall unified interface for the backend of the project.
class EditorMediator : public EditorEventSink,
                       public EditorPanelManager::Delegate,
                       public EditorTextActuator::Delegate,
                       public TabletModeObserver,
                       public KeyedService {
 public:
  // country_code that determines the country/territory in which the device is
  // situated.
  EditorMediator(Profile* profile, std::string_view country_code);
  ~EditorMediator() override;

  // Binds a new editor instance request from a client.
  void BindEditorClient(mojo::PendingReceiver<orca::mojom::EditorClient>
                            pending_receiver) override;

  // Binds a new panel manager request from a client.
  void BindEditorPanelManager(
      mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
          pending_receiver);

  // EditorEventSink overrides
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnActivateIme(std::string_view engine_id) override;
  void OnSurroundingTextChanged(const std::u16string& text,
                                gfx::Range selection_range) override;

  // EditorPanelManager::Delegate overrides
  void OnPromoCardDeclined() override;
  // TODO(b/301869966): Consider removing default parameters once the context
  // menu Orca entry is removed.
  void HandleTrigger(
      absl::optional<std::string_view> preset_query_id = absl::nullopt,
      absl::optional<std::string_view> freeform_text = absl::nullopt) override;
  EditorMode GetEditorMode() const override;
  // This method is currently used for metric purposes to understand the ratio
  // of requests being blocked vs. the potential requests that can be
  // accommodated.
  EditorOpportunityMode GetEditorOpportunityMode() const override;
  void CacheContext() override;

  // TabletModeObserver overrides
  void OnTabletModeStarting() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // EditorTextActuator::Delegate overrides
  void OnTextInserted() override;
  void ProcessConsentAction(ConsentAction consent_action) override;
  void ShowUI() override;
  void CloseUI() override;
  size_t GetSelectedTextLength() override;

  // KeyedService overrides
  void Shutdown() override;

  // Checks if the feature should be visible.
  bool IsAllowedForUse();

  EditorPanelManager* panel_manager() { return &panel_manager_; }

  bool SetTextQueryProviderResponseForTesting(
      const std::vector<std::string>& mock_results);

 private:
  struct SurroundingText {
    std::u16string text;
    gfx::Range selection_range;
  };

  void OnTextFieldContextualInfoChanged(const TextFieldContextualInfo& info);

  void SetUpNewEditorService();
  void BindEditor();
  void OnEditorServiceConnected(bool is_connection_bound);

  bool GetUserPref();
  void SetUserPref(bool value);

  // Not owned by this class
  raw_ptr<Profile> profile_;

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

  SurroundingText surrounding_text_;

  base::ScopedObservation<TabletMode, TabletModeObserver>
      tablet_mode_observation_{this};

  base::WeakPtrFactory<EditorMediator> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
