// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/prompt_suggestion.h"

#include <utility>

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

PromptSuggestion::PromptSuggestion(tabs::TabInterface& tab,
                                   std::u16string label,
                                   std::string prompt)
    : tab_(tab), label_(std::move(label)), prompt_(std::move(prompt)) {}

PromptSuggestion::~PromptSuggestion() = default;

const std::u16string& PromptSuggestion::GetLabel() const {
  return label_;
}

void PromptSuggestion::OnSuggestionPresented() {}

void PromptSuggestion::OnSuggestionExecuted() {
  if (!tab_->GetProfile()) {
    return;
  }
  GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
  if (service) {
    GlicInvokeOptions options(Target(*tab_),
                              mojom::InvocationSource::kTextSelectionWidget);
    options.prompts.push_back(prompt_);
    service->InvokeWithAutoSubmit(
        InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), std::move(options));
  }
}

::selection::mojom::ActionPtr PromptSuggestion::GetAction() const {
  return ::selection::mojom::Action::NewHandoff(
      ::selection::mojom::Handoff::New());
}

}  // namespace glic
