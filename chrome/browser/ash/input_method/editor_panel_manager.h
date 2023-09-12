// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::input_method {

enum class EditorPanelMode {
  // Editor panel should not be shown (due to consent rejected or other
  // restrictions).
  kBlocked = 0,
  // Editor panel should be shown in write mode.
  kWrite,
  // Editor panel should be shown in rewrite mode.
  kRewrite,
  // Editor panel feature promo card should be shown (to inform the user about
  // the feature and ask if they want to learn more).
  kPromoCard,
};

// Categories of preset text prompts to be shown on editor panel chips.
enum class EditorPanelPresetQueryCategory {
  kUnknown = 0,
};

struct EditorPanelPresetTextQuery {
  EditorPanelPresetTextQuery();
  EditorPanelPresetTextQuery(const EditorPanelPresetTextQuery&) = delete;
  EditorPanelPresetTextQuery& operator=(const EditorPanelPresetTextQuery&) =
      delete;
  ~EditorPanelPresetTextQuery();

  std::string text_query_id;
  std::string name;
  std::string description;
  EditorPanelPresetQueryCategory category =
      EditorPanelPresetQueryCategory::kUnknown;
};

// Context to determine what should be shown on the editor panel.
struct EditorPanelContext {
  EditorPanelContext();
  EditorPanelContext(const EditorPanelContext&) = delete;
  EditorPanelContext& operator=(const EditorPanelContext&) = delete;
  ~EditorPanelContext();

  EditorPanelMode editor_panel_mode = EditorPanelMode::kPromoCard;
  std::vector<EditorPanelPresetTextQuery> preset_text_queries;
};

// Interface to handle communication between the context menu editor panel entry
// point and the backend of the editor feature. This includes providing context
// to determine what should be shown on the editor panel and handling events
// from the editor panel.
class EditorPanelManager : public crosapi::mojom::EditorPanelManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnPromoCardActionReceived(
        PromoCardAction promo_card_action) = 0;
  };

  explicit EditorPanelManager(Delegate* delegate);
  EditorPanelManager(const EditorPanelManager&) = delete;
  EditorPanelManager& operator=(const EditorPanelManager&) = delete;
  ~EditorPanelManager() override;

  // Gets the context required to determine what should be shown on the editor
  // panel.
  void GetEditorPanelContext(GetEditorPanelContextCallback callback) override;

  // Should be called when a promo card is implicitly dismissed (e.g. the
  // user clicked out the promo card).
  void OnPromoCardDismissed() override;

  // Should be called when the promo card is explicitly dismissed via clicking
  // the button.
  void OnPromoCardDeclined() override;

  // Starts the editing flow, showing the consent form if needed.
  void StartEditingFlow() override;

  // Starts the rewrite flow with a preset text query.
  void StartEditingFlowWithPreset(const std::string& text_query_id) override;

  // Starts the write/rewrite flow with a freeform query.
  void StartEditingFlowWithFreeform(const std::string& text) override;

  void BindReceiver(mojo::PendingReceiver<crosapi::mojom::EditorPanelManager>
                        pending_receiver);

 private:
  raw_ptr<Delegate> delegate_;
  mojo::ReceiverSet<crosapi::mojom::EditorPanelManager> receivers_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_PANEL_MANAGER_H_
