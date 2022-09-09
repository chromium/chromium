// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_window_observer.h"

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
  delegate_->OnWindowDestroying(window);
}

void DlpWindowObserver::OnWindowOcclusionChanged(aura::Window* window) {
  DCHECK_EQ(window_, window);
  delegate_->OnWindowOcclusionChanged(window_);
}

void DlpWindowObserver::OnWindowTitleChanged(aura::Window* window) {
  DCHECK_EQ(window_, window);
  delegate_->OnWindowTitleChanged(window_);
}

}  // namespace policy
