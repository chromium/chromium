// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_CAST_CONFIG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_CAST_CONFIG_CONTROLLER_H_

#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/cast_config_controller.h"
#include "base/functional/callback.h"

namespace ash {

// A `CastConfigController` for tests. Allows testing code that calls
// `CastConfigController::Get()`, like the quick settings cast view.
class TestCastConfigController : public CastConfigController {
 public:
  TestCastConfigController();
  TestCastConfigController(const TestCastConfigController&) = delete;
  TestCastConfigController& operator=(const TestCastConfigController&) = delete;
  ~TestCastConfigController() override;

  void set_has_media_router(bool value) { has_media_router_ = value; }
  void set_has_sinks_and_routes(bool value) { has_sinks_and_routes_ = value; }
  void set_has_active_route(bool value) { has_active_route_ = value; }
  void set_access_code_casting_enabled(bool value) {
    access_code_casting_enabled_ = value;
  }
  void set_cast_to_sink_closure(base::OnceClosure value) {
    cast_to_sink_closure_ = std::move(value);
  }

  size_t cast_to_sink_count() const { return cast_to_sink_count_; }
  size_t stop_casting_count() const { return stop_casting_count_; }
  const std::string& stop_casting_route_id() const {
    return stop_casting_route_id_;
  }
  size_t freeze_route_count() const { return freeze_route_count_; }
  const std::string& freeze_route_route_id() const {
    return freeze_route_route_id_;
  }
  size_t unfreeze_route_count() const { return unfreeze_route_count_; }
  const std::string& unfreeze_route_route_id() const {
    return unfreeze_route_route_id_;
  }

  // Adds an entry to `sinks_and_routes_`.
  void AddSinkAndRoute(const SinkAndRoute& sink_and_route);

  // Resets the captured route IDs.
  void ResetRouteIds();

  // CastConfigController:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool HasMediaRouterForPrimaryProfile() const override;
  bool HasSinksAndRoutes() const override;
  bool HasActiveRoute() const override;
  bool AccessCodeCastingEnabled() const override;
  void RequestDeviceRefresh() override {}
  const std::vector<SinkAndRoute>& GetSinksAndRoutes() override;
  void CastToSink(const std::string& sink_id) override;
  void StopCasting(const std::string& route_id) override;
  void FreezeRoute(const std::string& route_id) override;
  void UnfreezeRoute(const std::string& route_id) override;

 private:
  bool has_media_router_ = true;
  bool has_sinks_and_routes_ = false;
  bool has_active_route_ = false;
  bool access_code_casting_enabled_ = false;
  // Must be a member because it is returned by reference.
  std::vector<SinkAndRoute> sinks_and_routes_;
  size_t cast_to_sink_count_ = 0;
  // Closure to run when `CastToSink()` is called.
  base::OnceClosure cast_to_sink_closure_;
  size_t stop_casting_count_ = 0;
  std::string stop_casting_route_id_;
  size_t freeze_route_count_ = 0;
  std::string freeze_route_route_id_;
  size_t unfreeze_route_count_ = 0;
  std::string unfreeze_route_route_id_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_CAST_CONFIG_CONTROLLER_H_
