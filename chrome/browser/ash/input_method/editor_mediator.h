// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_

#include <optional>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/input_method/editor_announcer.h"
#include "chrome/browser/ash/input_method/editor_client_connector.h"
#include "chrome/browser/ash/input_method/editor_consent_store.h"
#include "chrome/browser/ash/input_method/editor_event_proxy.h"
#include "chrome/browser/ash/input_method/editor_event_sink.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/input_method/editor_query_context.h"
#include "chrome/browser/ash/input_method/editor_service_connector.h"
#include "chrome/browser/ash/input_method/editor_switch.h"
#include "chrome/browser/ash/input_method/editor_system_actuator.h"
#include "chrome/browser/ash/input_method/editor_text_query_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/mako/mako_bubble_coordinator.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/display/display_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {
namespace input_method {

// Acts as a central "connector" for all things related to the orca project.
// This includes all current (and future) trigger points, providing the required
// plumbing to broker mojo connections from WebUIs and other clients, and
// providing an overall unified interface for the backend of the project.
class EditorMediator : public EditorContext::Observer,
                       public EditorContext::System,
                       public EditorEventSink,
                       public EditorPanelManager::Delegate,
                       public EditorSwitch::Observer,
                       public EditorSystemActuator::System,
                       public display::DisplayObserver,
                       public KeyedService {
 public:
  EditorMediator(
      Profile* profile,
      std::unique_ptr<EditorGeolocationProvider> editor_geolocation_provider);
  ~EditorMediator() override;

  // Binds a new editor instance request from a client.
  void BindEditorClient(mojo::PendingReceiver<orca::mojom::EditorClient>
                            pending_receiver) override;

  // Binds a new panel manager request from a client.
  void BindEditorPanelManager(
      mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
          pending_receiver);

  // EditorContext::Observer
  void OnContextUpdated() override;
  void OnImeChange(std::string_view engine_id) override;

  // EditorContext::System
  std::optional<ukm::SourceId> GetUkmSourceId() override;

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
      std::optional<std::string_view> preset_query_id = std::nullopt,
      std::optional<std::string_view> freeform_text = std::nullopt) override;
  EditorMode GetEditorMode() const override;
  ConsentStatus GetConsentStatus() const override;
  // This method is currently used for metric purposes to understand the ratio
  // of requests being blocked vs. the potential requests that can be
  // accommodated.
  EditorOpportunityMode GetEditorOpportunityMode() const override;
  std::vector<EditorBlockedReason> GetBlockedReasons() const override;
  void CacheContext() override;
  EditorMetricsRecorder* GetMetricsRecorder() override;

  // display::DisplayObserver overrides
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // EditorSystemActuator::System overrides
  void Announce(const std::u16string& message) override;
  // EditorSystemActuator::System / EditorPanelManager::Delegate override
  void ProcessConsentAction(ConsentAction consent_action) override;
  void ShowUI() override;
  void CloseUI() override;
  size_t GetSelectedTextLength() override;

  // EditorSwitch::Observer overrides
  void OnEditorModeChanged(const EditorMode& mode) override;

  // KeyedService overrides
  void Shutdown() override;

  void ShowNotice();

  // Checks if the feature should be visible.
  bool IsAllowedForUse();

  // Checks if the review notice banner can be shown in the settings page.
  bool CanShowNoticeBanner() const;

  EditorPanelManager* panel_manager() { return &panel_manager_; }

  MakoBubbleCoordinator& mako_bubble_coordinator_for_testing() {
    return mako_bubble_coordinator_;
  }

  bool SetTextQueryProviderResponseForTesting(
      const std::vector<std::string>& mock_results);
  void FetchAndUpdateInputContextForTesting();
  void OverrideEditorModeForTesting(EditorMode editor_mode);

 private:
  struct SurroundingText {
    std::u16string text;
    gfx::Range selection_range;
  };

  class ServiceConnection {
   public:
    ServiceConnection(Profile* profile,
                      EditorMediator* mediator,
                      EditorMetricsRecorder* metrics_recorder,
                      EditorServiceConnector* service_connector);
    ~ServiceConnection();

    EditorEventProxy* editor_event_proxy();
    EditorClientConnector* editor_client_connector();
    EditorTextQueryProvider* text_query_provider();
    EditorSystemActuator* system_actuator();

   private:
    std::unique_ptr<EditorEventProxy> editor_event_proxy_;
    std::unique_ptr<EditorClientConnector> editor_client_connector_;
    std::unique_ptr<EditorTextQueryProvider> text_query_provider_;
    std::unique_ptr<EditorSystemActuator> system_actuator_;
  };

  void OnTextFieldContextualInfoChanged(const TextFieldContextualInfo& info);
  void OnEditorServiceConnected(bool is_connection_bound);
  bool IsServiceConnected();
  void ResetEditorConnections();

  bool GetUserPref();
  void SetUserPref(bool value);

  // Not owned by this class
  raw_ptr<Profile> profile_;

  EditorPanelManager panel_manager_;
  std::unique_ptr<EditorGeolocationProvider> editor_geolocation_provider_;
  MakoBubbleCoordinator mako_bubble_coordinator_;
  EditorContext editor_context_;

  std::unique_ptr<EditorSwitch> editor_switch_;
  std::unique_ptr<EditorMetricsRecorder> metrics_recorder_;
  std::unique_ptr<EditorConsentStore> consent_store_;
  std::unique_ptr<EditorServiceConnector> editor_service_connector_;
  std::unique_ptr<ServiceConnection> service_connection_;
  EditorLiveRegionAnnouncer announcer_;
  SurroundingText surrounding_text_;

  std::optional<EditorMode> editor_mode_override_for_testing_;

  std::optional<EditorQueryContext> query_context_;

  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<EditorMediator> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
