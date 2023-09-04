// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/input_method/editor_consent_store.h"
#include "chrome/browser/ash/input_method/editor_event_sink.h"
#include "chrome/browser/ash/input_method/editor_instance_impl.h"
#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chrome/browser/ash/input_method/editor_switch.h"
#include "chrome/browser/ash/input_method/editor_text_actuator.h"
#include "chrome/browser/ash/input_method/mojom/editor.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {
namespace input_method {

// Acts as a central "connector" for all things related to the orca project.
// This includes all current (and future) trigger points, providing the required
// plumbing to broker mojo connections from WebUIs and other clients, and
// providing an overall unified interface for the backend of the project.
class EditorMediator : public EditorInstanceImpl::Delegate,
                       public EditorEventSink,
                       public ProfileObserver {
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
  void BindEditorInstance(
      mojo::PendingReceiver<mojom::EditorInstance> pending_receiver);

  // Handles a trigger event received from the system. This event could come
  // from a number of system locations.
  void HandleTrigger();

  // EditorEventSink
  void OnFocus(int context_id) override;
  void OnBlur() override;
  void OnActivateIme(std::string_view engine_id) override;
  void OnConsentActionReceived(ConsentAction consent_action) override;

  // EditorInstanceImpl::Delegate overrides
  void CommitEditorResult(std::string_view text) override;

  // Checks if the feature should be visible.
  bool IsAllowedForUse();

  // Checks if the feature can be triggered.
  bool CanBeTriggered();

  ConsentStatus GetConsentStatus();

  // ProfileObserver overrides:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  EditorPanelManager& panel_manager() { return panel_manager_; }

 private:
  void OnTextFieldContextualInfoChanged(const TextFieldContextualInfo& info);

  bool GetUserPref();
  void SetUserPref(bool value);

  // Not owned by this class
  raw_ptr<Profile> profile_;

  EditorInstanceImpl editor_instance_impl_;
  EditorTextActuator text_actuator_;
  EditorPanelManager panel_manager_;
  std::unique_ptr<EditorSwitch> editor_switch_;
  std::unique_ptr<EditorConsentStore> consent_store_;

  // May contain an instance of MakoPageHandler. This is used to control the
  // lifetime of the Mako WebUI.
  std::unique_ptr<ash::MakoPageHandler> mako_page_handler_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<EditorMediator> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_MEDIATOR_H_
