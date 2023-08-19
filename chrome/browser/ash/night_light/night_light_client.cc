// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/night_light/night_light_client.h"
#include "base/check_op.h"

namespace ash {

static NightLightClient* g_night_light_client = nullptr;

NightLightClient::NightLightClient() {
  CHECK_EQ(g_night_light_client, nullptr);
  g_night_light_client = this;
}

NightLightClient::~NightLightClient() {
  CHECK_NE(g_night_light_client, nullptr);
  g_night_light_client = nullptr;
}

NightLightClient* NightLightClient::Get() {
  CHECK_NE(g_night_light_client, nullptr);
  return g_night_light_client;
}

}  // namespace ash
