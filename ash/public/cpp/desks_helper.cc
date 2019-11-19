// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desks_helper.h"

#include "base/logging.h"

namespace ash {

namespace {
DesksHelper* g_instance = nullptr;
}  // namespace

// static
DesksHelper* DesksHelper::Get() {
  DCHECK(g_instance);
  return g_instance;
}

DesksHelper::DesksHelper() {
  DCHECK(!g_instance);
  g_instance = this;
}

DesksHelper::~DesksHelper() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
