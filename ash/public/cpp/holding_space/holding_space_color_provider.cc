// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_color_provider.h"

#include "base/check_op.h"

namespace ash {

namespace {

// The singleton instance.
HoldingSpaceColorProvider* g_instance = nullptr;

}  // namespace

HoldingSpaceColorProvider::~HoldingSpaceColorProvider() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
HoldingSpaceColorProvider* HoldingSpaceColorProvider::Get() {
  return g_instance;
}

HoldingSpaceColorProvider::HoldingSpaceColorProvider() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

}  // namespace ash
