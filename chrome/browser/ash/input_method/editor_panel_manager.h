// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_text_selection_mode.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::input_method {

using GetEditorPanelContextCallback =
    base::OnceCallback<void(const chromeos::editor_menu::EditorContext&)>;

class EditorPanelManager {
 public:
  virtual ~EditorPanelManager() = default;
  // Gets the context of editor feature required for the context menu card.
  virtual void GetEditorPanelContext(
      GetEditorPanelContextCallback callback) = 0;

  // Invoked when the promo card is implicitly dismissed (e.g. the
  // user clicked out of the promo card).
  virtual void OnPromoCardDismissed() = 0;

  // Invoked when the promo card is explicitly dismissed via clicking
  // the button.
  virtual void OnPromoCardDeclined() = 0;

  // Invoked when the consent is rejected by the user.
  virtual void OnConsentRejected() = 0;

  // Starts the editing flow, showing the notice screen if needed.
  virtual void StartEditingFlow() = 0;

  // Starts the rewrite flow with a preset text query, showing the notice screen
  // if needed.
  virtual void StartEditingFlowWithPreset(const std::string& text_query_id) = 0;

  // Starts the write/rewrite flow with a freeform query, showing the notice
  // screen if needed.
  virtual void StartEditingFlowWithFreeform(const std::string& text) = 0;

  // Invoked when the editor menu is shown or hidden.
  virtual void OnEditorMenuVisibilityChanged(bool visible) = 0;

  // Reports the mode of the editor panel.
  virtual void LogEditorMode(chromeos::editor_menu::EditorMode mode) = 0;

  // Invoked when the consent is approved by the user.
  virtual void OnConsentApproved() = 0;

  // Invoked when the magic boost promo card is explicitly declined by the user.
  virtual void OnMagicBoostPromoCardDeclined() = 0;

  // Determines if editor should require an opt-in.
  virtual bool ShouldOptInEditor() = 0;
};

// Interface to handle communication between the context menu editor panel entry
// point and the backend of the editor feature. This includes providing context
// to determine what should be shown on the editor panel and handling events
// from the editor panel.
class EditorPanelManagerImpl : public EditorPanelManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void BindEditorClient(
        mojo::PendingReceiver<orca::mojom::EditorClient> pending_receiver) = 0;

    virtual void OnPromoCardDeclined() = 0;
    virtual void ProcessConsentAction(ConsentAction consent_action) = 0;
    virtual void HandleTrigger(
        std::optional<std::string_view> preset_query_id,
        std::optional<std::string_view> freeform_text) = 0;
    virtual chromeos::editor_menu::EditorMode GetEditorMode() const = 0;
    virtual chromeos::editor_menu::EditorTextSelectionMode
    GetEditorTextSelectionMode() const = 0;
    virtual chromeos::editor_menu::EditorConsentStatus GetConsentStatus()
        const = 0;
    virtual EditorMetricsRecorder* GetMetricsRecorder() = 0;
    virtual EditorOpportunityMode GetEditorOpportunityMode() const = 0;
    virtual std::vector<EditorBlockedReason> GetBlockedReasons() const = 0;

    virtual void CacheContext() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEditorModeChanged(
        chromeos::editor_menu::EditorMode mode) = 0;
  };

  explicit EditorPanelManagerImpl(Delegate* delegate);
  EditorPanelManagerImpl(const EditorPanelManagerImpl&) = delete;
  EditorPanelManagerImpl& operator=(const EditorPanelManagerImpl&) = delete;
  ~EditorPanelManagerImpl() override;

  void GetEditorPanelContext(GetEditorPanelContextCallback callback) override;
  void OnPromoCardDismissed() override;
  void OnPromoCardDeclined() override;
  void OnConsentRejected() override;
  void StartEditingFlow() override;
  void StartEditingFlowWithPreset(const std::string& text_query_id) override;
  void StartEditingFlowWithFreeform(const std::string& text) override;
  void OnEditorMenuVisibilityChanged(bool visible) override;
  void LogEditorMode(chromeos::editor_menu::EditorMode mode) override;

  void BindEditorClient();
  bool IsEditorMenuVisible() const;
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void NotifyEditorModeChanged(chromeos::editor_menu::EditorMode mode);
  void RequestCacheContext();

  // Used by the Magic Boost opt-in flow. Virtual for testing.
  void OnConsentApproved() override;
  void OnMagicBoostPromoCardDeclined() override;

  bool ShouldOptInEditor() override;

  // For testing
  void SetEditorClientForTesting(
      mojo::PendingRemote<orca::mojom::EditorClient> client_for_testing);

 private:
  void OnGetPresetTextQueriesResult(
      GetEditorPanelContextCallback callback,
      chromeos::editor_menu::EditorMode panel_mode,
      std::vector<orca::mojom::PresetTextQueryPtr> queries);

  raw_ptr<Delegate> delegate_;
  mojo::Remote<orca::mojom::EditorClient> editor_client_remote_;

  bool is_editor_menu_visible_ = false;

  base::ObserverList<EditorPanelManagerImpl::Observer> observers_;

  base::WeakPtrFactory<EditorPanelManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_
