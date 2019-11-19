// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_interface_binder.h"

namespace ash {

AssistantInterfaceBinder* g_binder = nullptr;

AssistantInterfaceBinder::AssistantInterfaceBinder() {
  DCHECK(!g_binder);
  g_binder = this;
}

AssistantInterfaceBinder::~AssistantInterfaceBinder() {
  DCHECK_EQ(g_binder, this);
  g_binder = nullptr;
}

// static
AssistantInterfaceBinder* AssistantInterfaceBinder::GetInstance() {
  return g_binder;
}

}  // namespace ash
