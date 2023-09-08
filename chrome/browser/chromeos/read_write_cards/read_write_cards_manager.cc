// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/read_write_cards/read_write_cards_manager.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/views/editor_menu/editor_menu_controller_impl.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_card_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/context_menu_params.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

ReadWriteCardsManager::ReadWriteCardsManager()
    : quick_answers_controller_(
          std::make_unique<QuickAnswersControllerImpl>()) {
  quick_answers_controller_->SetClient(
      std::make_unique<quick_answers::QuickAnswersClient>(
          g_browser_process->shared_url_loader_factory(),
          quick_answers_controller_->GetQuickAnswersDelegate()));

  if (chromeos::features::IsOrcaEnabled()) {
    editor_menu_controller_ =
        std::make_unique<editor_menu::EditorMenuControllerImpl>();
  }
}

ReadWriteCardsManager::~ReadWriteCardsManager() = default;

void ReadWriteCardsManager::Shutdown() {
  editor_menu_controller_.reset();
  quick_answers_controller_.reset();
}

ReadWriteCardController* ReadWriteCardsManager::GetController(
    const content::ContextMenuParams& params,
    bool is_password_field) {
  // Currently only return QuickAnswersControllerImpl.
  if (!QuickAnswersState::Get()->is_eligible()) {
    return nullptr;
  }

  // Skip password input field.
  if (is_password_field) {
    return nullptr;
  }

  // Skip if no text selected.
  if (params.selection_text.empty()) {
    return nullptr;
  }

  return quick_answers_controller_.get();
}

}  // namespace chromeos
