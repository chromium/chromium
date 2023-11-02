// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_color_provider.h"

#include "base/check_op.h"

namespace ash {

namespace {

AppListColorProvider* g_instance = nullptr;

}  // namespace

// static
AppListColorProvider* AppListColorProvider::Get() {
  return g_instance;
}

AppListColorProvider::AppListColorProvider() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AppListColorProvider::~AppListColorProvider() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
