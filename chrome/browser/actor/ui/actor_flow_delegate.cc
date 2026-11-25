// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_flow_delegate.h"

namespace actor::ui {

ActorFlowDelegate::ActorFlowDelegate() = default;

ActorFlowDelegate::~ActorFlowDelegate() = default;

bool ActorFlowDelegate::ShouldShowBrowserUi() const {
  return false;
}

base::WeakPtr<ActorFlowDelegate> ActorFlowDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BrowserUiActorFlowDelegate::BrowserUiActorFlowDelegate() = default;

BrowserUiActorFlowDelegate::~BrowserUiActorFlowDelegate() = default;

bool BrowserUiActorFlowDelegate::ShouldShowBrowserUi() const {
  return true;
}

}  // namespace actor::ui
