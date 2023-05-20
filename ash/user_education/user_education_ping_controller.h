// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_PING_CONTROLLER_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_PING_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace views {
class View;
}  // namespace views

namespace ash {

enum class PingId;

// The singleton controller, owned by the `UserEducationController`, responsible
// for creation/management of pings.
class ASH_EXPORT UserEducationPingController {
 public:
  // Names for ping layers so they are easy to distinguish in debugging/testing.
  static constexpr char kPingParentLayerName[] = "Ping::Parent";
  static constexpr char kPingChildLayerName[] = "Ping::Child";

  UserEducationPingController();
  UserEducationPingController(const UserEducationPingController&) = delete;
  UserEducationPingController& operator=(const UserEducationPingController&) =
      delete;
  ~UserEducationPingController();

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if user education features are enabled.
  static UserEducationPingController* Get();

  // Attempts to creates a ping, identified by `ping_id`, for the specified
  // `view`, returning whether a ping was in fact created. Note that a ping will
  // *not* be created if:
  // (a) a ping already exists for the specified `ping_id`;
  // (b) a ping already exists for the specified `view`;
  // (c) the specified `view` is not drawn.
  bool CreatePing(PingId ping_id, views::View* view);

  // Returns the unique identifier for the ping currently being shown for the
  // specified `view`. If no ping is currently being shown for `view`, an absent
  // value is returned.
  absl::optional<PingId> GetPingId(const views::View* view) const;

 private:
  class Ping;

  // Owns pings, mapping them to their associated IDs. Note that pings are
  // removed from the map on ping animation ended/aborted.
  std::map<PingId, std::unique_ptr<Ping>> pings_by_id_;
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_PING_CONTROLLER_H_
