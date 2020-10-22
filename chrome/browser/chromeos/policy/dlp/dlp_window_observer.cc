// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_window_observer.h"

#include "ui/aura/window.h"

namespace policy {

DlpWindowObserver::DlpWindowObserver(aura::Window* window, Delegate* delegate)
    : window_(window), delegate_(delegate) {
  DCHECK(window_);
  window_->AddObserver(this);
}

DlpWindowObserver::~DlpWindowObserver() {
  if (window_)
    window_->RemoveObserver(this);
}

void DlpWindowObserver::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void DlpWindowObserver::OnWindowOcclusionChanged(aura::Window* window) {
  DCHECK_EQ(window_, window);
  delegate_->OnWindowOcclusionChanged(window_);
}

}  // namespace policy
