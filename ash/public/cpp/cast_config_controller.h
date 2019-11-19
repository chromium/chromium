// Copyright 2019 The Chromium Authors. All rights reserved.
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
// media_router::SinkIconType.
enum class SinkIconType {
  kCast = 0,
  kCastAudioGroup = 1,
  kCastAudio = 2,
  kMeeting = 3,
  kHangout = 4,
  kEducation = 5,
  kWiredDisplay = 6,
  kGeneric = 7,
};

struct ASH_PUBLIC_EXPORT CastSink {
  CastSink();
  CastSink(const CastSink& other);

  std::string id;
  std::string name;
  std::string domain;

  // Icon which describes the type of sink media is being routed to.
  SinkIconType sink_icon_type = SinkIconType::kGeneric;
};

enum class ContentSource {
  kUnknown,
  kTab,
  kDesktop,
};

struct ASH_PUBLIC_EXPORT CastRoute {
  std::string id;
  std::string title;

  // Is the activity source this computer? ie, are we mirroring the display?
  bool is_local_source = false;

  // What is source of the content? For example, we could be DIAL casting a
  // tab or mirroring the entire desktop.
  ContentSource content_source = ContentSource::kUnknown;
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

  // Return true if there are available cast devices.
  virtual bool HasSinksAndRoutes() const = 0;

  // Return true if casting is active. The route may be DIAL based, such as
  // casting YouTube where the cast sink directly streams content from another
  // server. In that case, this device is not actively transmitting information
  // to the cast sink.
  virtual bool HasActiveRoute() const = 0;

  // Request fresh data from the backend. When the data is available, all
  // registered observers will get called.
  virtual void RequestDeviceRefresh() = 0;

  virtual const std::vector<ash::SinkAndRoute>& GetSinksAndRoutes() = 0;

  // Initiate a casting session to the sink identified by |sink_id|.
  virtual void CastToSink(const std::string& sink_id) = 0;

  // A user-initiated request to stop the given cast session.
  virtual void StopCasting(const std::string& route_id) = 0;

 protected:
  CastConfigController();
  virtual ~CastConfigController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAST_CONFIG_CONTROLLER_H_
