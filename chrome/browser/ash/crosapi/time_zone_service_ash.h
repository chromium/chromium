// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TIME_ZONE_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_TIME_ZONE_SERVICE_ASH_H_

#include <string>

#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/crosapi/mojom/timezone.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implementation of TimeZoneService crosapi. Notifies the system timezone
// settings change to Lacros.
class TimeZoneServiceAsh : public mojom::TimeZoneService,
                           public ash::system::TimezoneSettings::Observer {
 public:
  TimeZoneServiceAsh();
  TimeZoneServiceAsh(const TimeZoneServiceAsh&) = delete;
  TimeZoneServiceAsh& operator=(const TimeZoneServiceAsh&) = delete;
  ~TimeZoneServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::TimeZoneService> receiver);

  // crosapi::mojom::TimeZoneService
  void AddObserver(
      mojo::PendingRemote<mojom::TimeZoneObserver> observer) override;

  // ash::system::TimezoneSettings::Observer
  void TimezoneChanged(const icu::TimeZone& timezone) override;

 private:
  // Cache of the current time zone ID.
  std::u16string time_zone_id_;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::TimeZoneService> receivers_;
  mojo::RemoteSet<mojom::TimeZoneObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TIME_ZONE_SERVICE_ASH_H_
