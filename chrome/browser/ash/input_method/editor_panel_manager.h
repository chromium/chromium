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
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::input_method {

// Interface to handle communication between the context menu editor panel entry
// point and the backend of the editor feature. This includes providing context
// to determine what should be shown on the editor panel and handling events
// from the editor panel.
class EditorPanelManager : public crosapi::mojom::EditorPanelManager {
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
    virtual EditorMode GetEditorMode() const = 0;
    virtual ConsentStatus GetConsentStatus() const = 0;
    virtual EditorMetricsRecorder* GetMetricsRecorder() = 0;
    virtual EditorOpportunityMode GetEditorOpportunityMode() const = 0;
    virtual std::vector<EditorBlockedReason> GetBlockedReasons() const = 0;

    virtual void CacheContext() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEditorModeChanged(const EditorMode& mode) = 0;
  };

  explicit EditorPanelManager(Delegate* delegate);
  EditorPanelManager(const EditorPanelManager&) = delete;
  EditorPanelManager& operator=(const EditorPanelManager&) = delete;
  ~EditorPanelManager() override;

  // crosapi::mojom::EditorPanelManager:
  void GetEditorPanelContext(GetEditorPanelContextCallback callback) override;
  void OnPromoCardDismissed() override;
  void OnPromoCardDeclined() override;
  void OnConsentRejected() override;
  void StartEditingFlow() override;
  void StartEditingFlowWithPreset(const std::string& text_query_id) override;
  void StartEditingFlowWithFreeform(const std::string& text) override;
  void OnEditorMenuVisibilityChanged(bool visible) override;
  void LogEditorMode(crosapi::mojom::EditorPanelMode mode) override;
  void BindEditorObserver(mojo::PendingRemote<crosapi::mojom::EditorObserver>
                              pending_observer_remote) override;

  void BindReceiver(mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
                        pending_receiver);
  void BindEditorClient();
  bool IsEditorMenuVisible() const;
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void NotifyEditorModeChanged(const EditorMode& mode);
  void RequestCacheContext();

  // Used by the Magic Boost opt-in flow. Virtual for testing.
  virtual void OnConsentApproved();
  void OnMagicBoostPromoCardDeclined();

  // For testing
  void SetEditorClientForTesting(
      mojo::PendingRemote<orca::mojom::EditorClient> client_for_testing);

 private:
  void OnGetPresetTextQueriesResult(
      GetEditorPanelContextCallback callback,
      crosapi::mojom::EditorPanelMode panel_mode,
      std::vector<orca::mojom::PresetTextQueryPtr> queries);

  raw_ptr<Delegate> delegate_;
  mojo::ReceiverSet<crosapi::mojom::EditorPanelManager> receivers_;
  mojo::Remote<orca::mojom::EditorClient> editor_client_remote_;

  bool is_editor_menu_visible_ = false;

  mojo::RemoteSet<crosapi::mojom::EditorObserver> observer_remotes_;
  base::ObserverList<EditorPanelManager::Observer> observers_;

  base::WeakPtrFactory<EditorPanelManager> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_
