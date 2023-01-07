// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/time_zone_service_ash.h"

#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace crosapi {

TimeZoneServiceAsh::TimeZoneServiceAsh() {
  auto* timezone_settings = ash::system::TimezoneSettings::GetInstance();
  time_zone_id_ = timezone_settings->GetCurrentTimezoneID();
  timezone_settings->AddObserver(this);
}

TimeZoneServiceAsh::~TimeZoneServiceAsh() {
  ash::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void TimeZoneServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::TimeZoneService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void TimeZoneServiceAsh::AddObserver(
    mojo::PendingRemote<mojom::TimeZoneObserver> observer) {
  auto id = observers_.Add(std::move(observer));
  observers_.Get(id)->OnTimeZoneChanged(time_zone_id_);
}

void TimeZoneServiceAsh::TimezoneChanged(const icu::TimeZone& timezone) {
  time_zone_id_ = ash::system::TimezoneSettings::GetTimezoneID(timezone);
  for (auto& observer : observers_)
    observer->OnTimeZoneChanged(time_zone_id_);
}

}  // namespace crosapi
