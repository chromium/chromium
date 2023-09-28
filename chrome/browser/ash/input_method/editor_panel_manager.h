// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    virtual void HandleTrigger(
        absl::optional<std::string_view> preset_query_id,
        absl::optional<std::string_view> freeform_text) = 0;
    virtual EditorMode GetEditorMode() const = 0;
  };

  explicit EditorPanelManager(Delegate* delegate);
  EditorPanelManager(const EditorPanelManager&) = delete;
  EditorPanelManager& operator=(const EditorPanelManager&) = delete;
  ~EditorPanelManager() override;

  // crosapi::mojom::EditorPanelManager:
  void GetEditorPanelContext(GetEditorPanelContextCallback callback) override;
  void OnPromoCardDismissed() override;
  void OnPromoCardDeclined() override;
  void StartEditingFlow() override;
  void StartEditingFlowWithPreset(const std::string& text_query_id) override;
  void StartEditingFlowWithFreeform(const std::string& text) override;
  void OnEditorMenuVisibilityChanged(bool visible) override;

  void BindReceiver(mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
                        pending_receiver);

  void BindEditorClient();

  bool IsEditorMenuVisible() const;

 private:
  void OnGetPresetTextQueriesResult(
      GetEditorPanelContextCallback callback,
      std::vector<orca::mojom::PresetTextQueryPtr> queries);

  raw_ptr<Delegate> delegate_;
  mojo::ReceiverSet<crosapi::mojom::EditorPanelManager> receivers_;
  mojo::Remote<orca::mojom::EditorClient> editor_client_remote_;

  bool is_editor_menu_visible_ = false;

  base::WeakPtrFactory<EditorPanelManager> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_
