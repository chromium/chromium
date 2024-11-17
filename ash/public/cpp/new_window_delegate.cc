// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/new_window_delegate.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {
namespace {
NewWindowDelegate* g_new_window_delegate = nullptr;
}

// static
NewWindowDelegate* NewWindowDelegate::GetInstance() {
  return g_new_window_delegate;
}

// static
NewWindowDelegate* NewWindowDelegate::GetPrimary() {
  return g_new_window_delegate;
}

NewWindowDelegate::NewWindowDelegate() {
  CHECK(!g_new_window_delegate);
  g_new_window_delegate = this;
}

NewWindowDelegate::~NewWindowDelegate() {
  CHECK_EQ(this, g_new_window_delegate);
  g_new_window_delegate = nullptr;
}

}  // namespace ash
