// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_cast_config_controller.h"

#include <utility>

#include "base/functional/callback.h"

namespace ash {

TestCastConfigController::TestCastConfigController() = default;

TestCastConfigController::~TestCastConfigController() = default;

void TestCastConfigController::AddSinkAndRoute(
    const SinkAndRoute& sink_and_route) {
  sinks_and_routes_.push_back(sink_and_route);
}

void TestCastConfigController::ResetRouteIds() {
  stop_casting_route_id_.clear();
  freeze_route_route_id_.clear();
  unfreeze_route_route_id_.clear();
}

bool TestCastConfigController::HasMediaRouterForPrimaryProfile() const {
  return has_media_router_;
}

bool TestCastConfigController::HasSinksAndRoutes() const {
  return has_sinks_and_routes_;
}

bool TestCastConfigController::HasActiveRoute() const {
  return has_active_route_;
}

bool TestCastConfigController::AccessCodeCastingEnabled() const {
  return access_code_casting_enabled_;
}

const std::vector<SinkAndRoute>& TestCastConfigController::GetSinksAndRoutes() {
  return sinks_and_routes_;
}

void TestCastConfigController::CastToSink(const std::string& sink_id) {
  ++cast_to_sink_count_;
  if (cast_to_sink_closure_) {
    std::move(cast_to_sink_closure_).Run();
  }
}

void TestCastConfigController::StopCasting(const std::string& route_id) {
  ++stop_casting_count_;
  stop_casting_route_id_ = route_id;
}

void TestCastConfigController::FreezeRoute(const std::string& route_id) {
  ++freeze_route_count_;
  freeze_route_route_id_ = route_id;
}

void TestCastConfigController::UnfreezeRoute(const std::string& route_id) {
  ++unfreeze_route_count_;
  unfreeze_route_route_id_ = route_id;
}

}  // namespace ash
