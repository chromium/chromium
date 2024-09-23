// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_STATUS_H_
#define ASH_SYSTEM_POWER_POWER_STATUS_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
struct VectorIcon;
}

namespace ui {
class ColorProvider;
}

namespace ash {

struct BatteryColorTemplate {
  bool operator==(const BatteryColorTemplate& other) const {
    bool foreground_colors_match = foreground_color == other.foreground_color;
    bool badge_colors_match = badge_color == other.badge_color;

    return foreground_colors_match && badge_colors_match;
  }

  bool operator!=(const BatteryColorTemplate& other) const {
    return !(*this == other);
  }

  SkColor foreground_color;
  std::optional<SkColor> badge_color;
};

struct BatteryColors {
  SkColor foreground_color;
  SkColor badge_color;
  SkColor alert_color;
};

// PowerStatus is a singleton that receives updates about the system's
// power status from chromeos::PowerManagerClient and makes the information
// available to interested classes within Ash.
class ASH_EXPORT PowerStatus : public chromeos::PowerManagerClient::Observer {
 public:
  // Interface for classes that wish to be notified when the power status
  // has changed.
  class Observer {
   public:
    // Called when the power status changes.
    virtual void OnPowerStatusChanged() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Power source types.
  enum DeviceType {
    // Dedicated charger (AC adapter, USB power supply, etc.).
    DEDICATED_CHARGER,

    // Dual-role device.
    DUAL_ROLE_USB,
  };

  // Information about an available power source.
  struct PowerSource {
    // ID provided by kernel.
    std::string id;

    // Type of power source.
    DeviceType type;

    // Message ID of a description for this port.
    int description_id;
  };

  // Information about the battery image corresponding to the status at a given
  // point in time. This can be cached and later compared to avoid unnecessarily
  // updating onscreen icons (GetBatteryImage() creates a new image on each
  // call).
  struct BatteryImageInfo {
    explicit BatteryImageInfo(
        const SkColor fg_color,
        const std::optional<SkColor> badge_color = std::nullopt)
        : icon_badge(nullptr),
          badge_outline(nullptr),
          alert_if_low(false),
          charge_percent(-1),
          battery_color_preferences({fg_color, badge_color}) {}

    static BatteryColors ResolveColors(const BatteryImageInfo& info,
                                       const ui::ColorProvider* color_provider);
    // Returns true if |this| and |o| are similar enough in terms of the image
    // they'd generate.
    bool ApproximatelyEqual(const BatteryImageInfo& o) const;

    // The badge (lightning bolt, exclamation mark, etc) that should be drawn
    // on top of the battery icon.
    raw_ptr<const gfx::VectorIcon> icon_badge;

    // The outline for the badge, need to draw this to satisfy contrast
    // requirements.
    raw_ptr<const gfx::VectorIcon> badge_outline;

    // When true and |charge_percent| is very low, special colors will be used
    // to alert the user.
    bool alert_if_low;

    double charge_percent;

    BatteryColorTemplate battery_color_preferences;
  };

  // Maximum battery time-to-full or time-to-empty that should be displayed
  // in the UI. If the current is close to zero, battery time estimates can
  // get very large; avoid displaying these large numbers.
  static const int kMaxBatteryTimeToDisplaySec;

  // An alert_if_low badge is drawn over the battery icon if the battery is not
  // connected to a charger and has less than |kCriticalBatteryChargePercentage|
  // percentage of charge remaining.
  static const double kCriticalBatteryChargePercentage;

  PowerStatus(const PowerStatus&) = delete;
  PowerStatus& operator=(const PowerStatus&) = delete;

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static PowerStatus* Get();

  power_manager::PowerSupplyProperties_ExternalPower external_power() const {
    return proto_.external_power();
  }

  // Adds or removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Requests updated status from the power manager.
  void RequestStatusUpdate();

  // Returns true if a battery is present.
  virtual bool IsBatteryPresent() const;

  // Returns true if the battery is full. This also implies that a charger
  // is connected.
  bool IsBatteryFull() const;

  // Returns true if the battery is charging. Note that this implies that a
  // charger is connected but the converse is not necessarily true: the
  // battery may be discharging even while a (perhaps low-power) charger is
  // connected. Use Is*Connected() to test for the presence of a charger
  // and also see IsBatteryDischargingOnLinePower().
  bool IsBatteryCharging() const;

  // Returns true if the battery is discharging (or neither charging nor
  // discharging while not being full) while line power is connected.
  bool IsBatteryDischargingOnLinePower() const;

  // Returns the battery's remaining charge as a value in the range [0.0,
  // 100.0].
  virtual double GetBatteryPercent() const;

  // Returns the battery's remaining charge, rounded to an integer with a
  // maximum value of 100.
  int GetRoundedBatteryPercent() const;

  // Returns true if the battery's time-to-full and time-to-empty estimates
  // should not be displayed because the power manager is still calculating
  // them.
  bool IsBatteryTimeBeingCalculated() const;

  // Returns the estimated time until the battery is empty (if line power
  // is disconnected) or full (if line power is connected). These estimates
  // should only be used if IsBatteryTimeBeingCalculated() returns false.
  //
  // Irrespective of IsBatteryTimeBeingCalculated(), estimates may be
  // unavailable if powerd didn't provide them because the battery current was
  // close to zero (resulting in time estimates approaching infinity).
  std::optional<base::TimeDelta> GetBatteryTimeToEmpty() const;
  std::optional<base::TimeDelta> GetBatteryTimeToFull() const;

  // Returns true if line power (including a charger of any type) is connected.
  virtual bool IsLinePowerConnected() const;

  // Returns true if an official, non-USB charger is connected.
  bool IsMainsChargerConnected() const;

  // Returns true if a USB charger (which is likely to only support a low
  // charging rate) is connected.
  virtual bool IsUsbChargerConnected() const;

  // Returns true if the system allows some connected devices to function as
  // either power sources or sinks.
  bool SupportsDualRoleDevices() const;

  // Returns true if at least one dual-role device is connected.
  bool HasDualRoleDevices() const;

  // Returns a list of available power sources which the user may select.
  std::vector<PowerSource> GetPowerSources() const;

  // Returns the ID of the currently used power source, or an empty string if no
  // power source is selected.
  std::string GetCurrentPowerSourceID() const;

  // Creates a new BatteryImageInfo struct, and populates the struct with
  // information related to the current Battery Status (i.e. the colors
  // used to draw the battery icon, the charge percent of the battery, etc).
  BatteryImageInfo GenerateBatteryImageInfo(
      const SkColor foreground_color,
      const std::optional<SkColor> badge_color = std::nullopt) const;

  // Creates a new image that should be shown for the battery's current state.
  static ui::ImageModel GetBatteryImageModel(const BatteryImageInfo& info,
                                             int height);

  // DEPRECATED: Use `GetBatteryImageModel()` instead.
  // Creates a new image that should be shown for the battery's current state.
  static gfx::ImageSkia GetBatteryImage(
      const BatteryImageInfo& info,
      int height,
      const ui::ColorProvider* color_provider);

  // Returns a string describing the current state for accessibility.
  std::u16string GetAccessibleNameString(bool full_description) const;

  // Returns status strings that are generated by current PowerStatus.
  // The first string is percentage e.g. "53%" and the second one is status in
  // words e.g. "5:00 left". Depending on the status, one of them may return
  // empty string.
  std::pair<std::u16string, std::u16string> GetStatusStrings() const;

  // Returns status strings that are generated by current PowerStatus.
  // For example, "53% - 5:00 left".
  std::u16string GetInlinedStatusString() const;

  // Returns the device's preferred minimum power input in watts (W).
  double GetPreferredMinimumPower() const;

  // Returns true if battery saver is active.
  virtual bool IsBatterySaverActive() const;

  // TODO(b/327054689): This pointer is needed because some power tests delete
  // PowerStatus without the observers knowing about it, so observers have to
  // check for its validity before using it.
  base::WeakPtr<PowerStatus> GetWeakPtr();

  // Updates |proto_|. Does not notify observers.
  void SetProtoForTesting(const power_manager::PowerSupplyProperties& proto);

  // Updates |battery_saver_active_|. Does not notify observers.
  void SetBatterySaverStateForTesting(bool active);

 protected:
  PowerStatus();
  ~PowerStatus() override;

 private:
  friend class ScopedFakePowerStatus;

  // Global singleton instance of the PowerStatus
  static PowerStatus* g_power_status_;

  // Overridden from PowerManagerClient::Observer.
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;
  void BatterySaverModeStateChanged(
      const power_manager::BatterySaverModeState& state) override;

  // A helper function called by GetBatteryImageInfo(). Populates the fields of
  // |info|.
  void CalculateBatteryImageInfo(BatteryImageInfo* info) const;

  // Callback used to query battery saver state from PowerManagerClient on
  // startup.
  void OnGotBatterySaverState(
      std::optional<power_manager::BatterySaverModeState> state);

  base::ObserverList<Observer>::Unchecked observers_;

  // Current state.
  power_manager::PowerSupplyProperties proto_;

  // Has proto_ been set with a value from Power Manager yet?
  bool proto_initialized_{false};

  // Current state of battery saver, queried on startup and updated in
  // BatterySaverModeStateChanged.
  bool battery_saver_active_{false};

  base::WeakPtrFactory<PowerStatus> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_STATUS_H_
