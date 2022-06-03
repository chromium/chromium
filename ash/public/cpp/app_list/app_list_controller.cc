// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

AppListController* g_instance = nullptr;

}  // namespace

// static
AppListController* AppListController::Get() {
  return g_instance;
}

AppListController::AppListController() {
  DCHECK(!g_instance);
  g_instance = this;
}

AppListController::~AppListController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
