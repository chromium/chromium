// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAST_CONFIG_CONTROLLER_H_
#define ASH_PUBLIC_CPP_CAST_CONFIG_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// The type of icon the sink is associated with. These values match
// media_router::SinkIconType and media_router::mojom::SinkIconType.
enum class SinkIconType {
  kCast = 0,
  kCastAudioGroup = 1,
  kCastAudio = 2,
  kWiredDisplay = 6,
  kGeneric = 7,
};

struct ASH_PUBLIC_EXPORT CastSink {
  CastSink();
  CastSink(const CastSink& other);

  std::string id;
  std::string name;

  // Icon which describes the type of sink media is being routed to.
  SinkIconType sink_icon_type = SinkIconType::kGeneric;
};

enum class ContentSource {
  kUnknown,
  kTab,
  kDesktop,
};

struct FreezeInfo {
  bool can_freeze = false;
  bool is_frozen = false;
};

struct ASH_PUBLIC_EXPORT CastRoute {
  std::string id;
  std::string title;

  // Is the activity source this computer? ie, are we mirroring the display?
  bool is_local_source = false;

  // What is source of the content? For example, we could be DIAL casting a
  // tab or mirroring the entire desktop.
  ContentSource content_source = ContentSource::kUnknown;

  // The state of freeze for the route. Is the route able to be frozen, and
  // is it currently frozen?
  FreezeInfo freeze_info;
};

struct ASH_PUBLIC_EXPORT SinkAndRoute {
  SinkAndRoute();
  SinkAndRoute(const SinkAndRoute& other);
  SinkAndRoute(SinkAndRoute&& other);

  CastSink sink;
  CastRoute route;
};

// This interface allows the UI code in ash, e.g. |TrayCastDetailedView|, to
// access the cast system. This is implemented in Chrome and is expected to
// outlive ash::Shell.
class ASH_PUBLIC_EXPORT CastConfigController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) = 0;

   protected:
    ~Observer() override = default;
  };

  // Returns the singleton instance, which may be null in unit tests.
  static CastConfigController* Get();

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true if the C++ MediaRouter service exists for the primary profile
  // and is not disabled by policy.
  virtual bool HasMediaRouterForPrimaryProfile() const = 0;

  // Return true if there are available cast devices.
  virtual bool HasSinksAndRoutes() const = 0;

  // Return true if casting is active. The route may be DIAL based, such as
  // casting YouTube where the cast sink directly streams content from another
  // server. In that case, this device is not actively transmitting information
  // to the cast sink.
  virtual bool HasActiveRoute() const = 0;

  // Returns true if access code casting is enabled for this user. This is
  // important because if it is enabled, the cast icon may need to be shown even
  // if there are no currently available sinks.
  virtual bool AccessCodeCastingEnabled() const = 0;

  // Request fresh data from the backend. When the data is available, all
  // registered observers will get called.
  virtual void RequestDeviceRefresh() = 0;

  virtual const std::vector<SinkAndRoute>& GetSinksAndRoutes() = 0;

  // Initiate a casting session to the sink identified by |sink_id|.
  virtual void CastToSink(const std::string& sink_id) = 0;

  // A user-initiated request to stop the given cast session.
  virtual void StopCasting(const std::string& route_id) = 0;

  // Freezes and Unfreezes a cast mirroring route (Displayed to users as
  // pause/resume).
  virtual void FreezeRoute(const std::string& route_id) = 0;
  virtual void UnfreezeRoute(const std::string& route_id) = 0;

 protected:
  CastConfigController();
  virtual ~CastConfigController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAST_CONFIG_CONTROLLER_H_
