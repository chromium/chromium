// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_ASSOCIATED_RULE_BASED_ENGINE_H_
#define ASH_SERVICES_IME_ASSOCIATED_RULE_BASED_ENGINE_H_

#include "ash/services/ime/input_engine.h"
#include "ash/services/ime/public/cpp/rulebased/engine.h"
#include "ash/services/ime/public/cpp/suggestions.h"
#include "ash/services/ime/public/mojom/input_method.mojom.h"
#include "ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chromeos {
namespace ime {

// Handles rule-based input methods such as Arabic and Vietnamese.
// Rule-based input methods are based off deterministic rules and do not
// provide features such as suggestions.
class AssociatedRuleBasedEngine : public InputEngine,
                                  public ash::ime::mojom::InputMethod {
 public:
  // Returns nullptr if |ime_spec| is not valid for this RuleBasedEngine.
  static std::unique_ptr<AssociatedRuleBasedEngine> Create(
      const std::string& ime_spec,
      mojo::PendingAssociatedReceiver<ash::ime::mojom::InputMethod> receiver,
      mojo::PendingAssociatedRemote<ash::ime::mojom::InputMethodHost> host);

  AssociatedRuleBasedEngine(const AssociatedRuleBasedEngine& other) = delete;
  AssociatedRuleBasedEngine& operator=(const AssociatedRuleBasedEngine& other) =
      delete;
  ~AssociatedRuleBasedEngine() override;

  // InputEngine:
  bool IsConnected() override;

  // mojom::InputMethod overrides:
  // Most of these methods are deliberately empty because rule-based input
  // methods do not need to listen to these events.
  void OnFocusDeprecated(
      ash::ime::mojom::InputFieldInfoPtr input_field_info,
      ash::ime::mojom::InputMethodSettingsPtr settings) override {}
  void OnFocus(ash::ime::mojom::InputFieldInfoPtr input_field_info,
               ash::ime::mojom::InputMethodSettingsPtr settings,
               OnFocusCallback callback) override;
  void OnBlur() override {}
  void OnSurroundingTextChanged(
      const std::string& text,
      uint32_t offset,
      ash::ime::mojom::SelectionRangePtr selection_range) override {}
  void OnCompositionCanceledBySystem() override;
  void ProcessKeyEvent(ash::ime::mojom::PhysicalKeyEventPtr event,
                       ProcessKeyEventCallback callback) override;
  void OnCandidateSelected(uint32_t selected_candidate_index) override;

  // TODO(https://crbug.com/837156): Implement a state for the interface.

 private:
  AssociatedRuleBasedEngine(
      const std::string& ime_spec,
      mojo::PendingAssociatedReceiver<ash::ime::mojom::InputMethod> receiver,
      mojo::PendingAssociatedRemote<ash::ime::mojom::InputMethodHost> host);

  mojo::AssociatedReceiver<ash::ime::mojom::InputMethod> receiver_;
  mojo::AssociatedRemote<ash::ime::mojom::InputMethodHost> host_;

  rulebased::Engine engine_;

  // Whether the AltRight key is held down or not.
  bool is_alt_right_key_down_ = false;
};

}  // namespace ime
}  // namespace chromeos

#endif  // ASH_SERVICES_IME_ASSOCIATED_RULE_BASED_ENGINE_H_
