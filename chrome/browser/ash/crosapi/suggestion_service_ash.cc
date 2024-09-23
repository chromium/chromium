// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/suggestion_service_ash.h"
#include "base/functional/bind.h"

namespace crosapi {

SuggestionServiceAsh::SuggestionServiceAsh() = default;

SuggestionServiceAsh::~SuggestionServiceAsh() = default;

void SuggestionServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::SuggestionService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SuggestionServiceAsh::AddSuggestionServiceProvider(
    mojo::PendingRemote<mojom::SuggestionServiceProvider> provider) {
  remotes_.Add(
      mojo::Remote<mojom::SuggestionServiceProvider>(std::move(provider)));
}

void SuggestionServiceAsh::GetTabSuggestionItems(
    TabSuggestionItemsCallback callback) {
  for (const auto& remote : remotes_) {
    remote->GetTabSuggestionItems(std::move(callback));
  }
}

}  // namespace crosapi
