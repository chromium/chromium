// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/desks_helper.h"

#include "base/check_op.h"
#include "base/time/time.h"

namespace ash {

namespace {
DesksHelper* g_instance = nullptr;
}  // namespace

DeskTemplate::DeskTemplate() : uuid_(base::Time::Now().ToDoubleT()) {}
DeskTemplate::DeskTemplate(double uuid) : uuid_(uuid) {}
DeskTemplate::~DeskTemplate() = default;

// static
DesksHelper* DesksHelper::Get() {
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
