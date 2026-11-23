// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion.h"

namespace selection {

Suggestion::Suggestion() = default;
Suggestion::~Suggestion() = default;

void Suggestion::Execute(mojo::GenericPendingAssociatedReceiver endpoint) {
  if (binder_ && endpoint.is_valid()) {
    binder_.Run(std::move(endpoint));
  }
  OnSuggestionExecuted();
}

}  // namespace selection

