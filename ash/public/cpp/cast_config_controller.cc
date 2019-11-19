// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/cast_config_controller.h"

#include "base/logging.h"

namespace ash {

namespace {
CastConfigController* g_instance = nullptr;
}

CastSink::CastSink() = default;

CastSink::CastSink(const CastSink& other) = default;

SinkAndRoute::SinkAndRoute() = default;

SinkAndRoute::SinkAndRoute(const SinkAndRoute& other) = default;

SinkAndRoute::SinkAndRoute(SinkAndRoute&& other) = default;

// static
CastConfigController* CastConfigController::Get() {
  return g_instance;
}

CastConfigController::CastConfigController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

CastConfigController::~CastConfigController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
