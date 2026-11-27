// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/target.h"

#include "chrome/browser/dictation/dictation_context_fetcher.h"

namespace dictation {

std::optional<DictationUrlCategory> Target::GetUrlCategory() const {
  return std::nullopt;
}

std::unique_ptr<DictationContextFetcher> Target::CreateContextFetcher() {
  return nullptr;
}

void Target::SetSelectedText(std::u16string selected_text) {}

std::optional<content::GlobalDOMNodeId> Target::GetGlobalDOMNodeId() const {
  return std::nullopt;
}

void Target::SetDelegate(Delegate* delegate) {}

void Target::SetStreamActive(bool active) {}

DomTarget* Target::AsDomTarget() {
  return nullptr;
}

const DomTarget* Target::AsDomTarget() const {
  return nullptr;
}

}  // namespace dictation
