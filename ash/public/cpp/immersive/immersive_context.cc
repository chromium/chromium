// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/immersive/immersive_context.h"

#include "base/logging.h"

namespace ash {

namespace {

ImmersiveContext* g_instance = nullptr;

}  // namespace

ImmersiveContext::~ImmersiveContext() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
ImmersiveContext* ImmersiveContext::Get() {
  return g_instance;
}

ImmersiveContext::ImmersiveContext() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

}  // namespace ash
