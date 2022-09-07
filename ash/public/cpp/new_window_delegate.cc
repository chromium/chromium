// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/new_window_delegate.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {
namespace {
NewWindowDelegateProvider* g_delegate_provider = nullptr;
}

// static
NewWindowDelegate* NewWindowDelegate::GetInstance() {
  if (!g_delegate_provider)
    return nullptr;
  return g_delegate_provider->GetInstance();
}

// static
NewWindowDelegate* NewWindowDelegate::GetPrimary() {
  if (!g_delegate_provider)
    return nullptr;
  return g_delegate_provider->GetPrimary();
}

NewWindowDelegate::NewWindowDelegate() = default;

NewWindowDelegate::~NewWindowDelegate() = default;

NewWindowDelegateProvider::NewWindowDelegateProvider() {
  DCHECK(!g_delegate_provider);
  g_delegate_provider = this;
}

NewWindowDelegateProvider::~NewWindowDelegateProvider() {
  DCHECK_EQ(g_delegate_provider, this);
  g_delegate_provider = nullptr;
}

}  // namespace ash
