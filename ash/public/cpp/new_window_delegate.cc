// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/new_window_delegate.h"

#include "base/logging.h"

namespace ash {
namespace {
NewWindowDelegate* g_new_window_delegate = nullptr;
}

// static
NewWindowDelegate* NewWindowDelegate::GetInstance() {
  return g_new_window_delegate;
}

NewWindowDelegate::NewWindowDelegate() {
  DCHECK(!g_new_window_delegate);
  g_new_window_delegate = this;
}

NewWindowDelegate::~NewWindowDelegate() {
  DCHECK_EQ(g_new_window_delegate, this);
  g_new_window_delegate = nullptr;
}

}  // namespace ash
