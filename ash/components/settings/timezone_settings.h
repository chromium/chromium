// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SETTINGS_TIMEZONE_SETTINGS_H_
#define ASH_COMPONENTS_SETTINGS_TIMEZONE_SETTINGS_H_

#include <string>
#include <vector>

#include "ash/components/settings/cros_settings_provider.h"
#include "base/component_export.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {
namespace system {

// Canonical name of UTC timezone.
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kUTCTimezoneName[];

// This interface provides access to Chrome OS timezone settings.
class COMPONENT_EXPORT(ASH_SETTINGS) TimezoneSettings {
 public:
  class Observer {
   public:
    // Called when the timezone has changed. |timezone| is non-null.
    virtual void TimezoneChanged(const icu::TimeZone& timezone) = 0;
   protected:
    virtual ~Observer();
  };

  static TimezoneSettings* GetInstance();

  // Returns the current timezone as an icu::Timezone object.
  virtual const icu::TimeZone& GetTimezone() = 0;
  virtual std::u16string GetCurrentTimezoneID() = 0;

  // Sets the current timezone and notifies all Observers.
  virtual void SetTimezone(const icu::TimeZone& timezone) = 0;
  virtual void SetTimezoneFromID(const std::u16string& timezone_id) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual const std::vector<std::unique_ptr<icu::TimeZone>>& GetTimezoneList()
      const = 0;

  // Gets timezone ID which is also used as timezone pref value.
  static std::u16string GetTimezoneID(const icu::TimeZone& timezone);

 protected:
  virtual ~TimezoneSettings() {}
};

}  // namespace system
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
namespace system {
using ::ash::system::TimezoneSettings;
}  // namespace system
}  // namespace chromeos

#endif  // ASH_COMPONENTS_SETTINGS_TIMEZONE_SETTINGS_H_
