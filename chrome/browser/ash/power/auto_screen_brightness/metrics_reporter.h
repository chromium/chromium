// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_METRICS_REPORTER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_METRICS_REPORTER_H_

#include <array>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/metrics/daily_event.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {
namespace power {
namespace auto_screen_brightness {

// MetricsReport logs daily user screen brightness adjustments to UMA.
class MetricsReporter : public chromeos::PowerManagerClient::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DeviceClass {
    kNoAls = 0,
    kSupportedAls = 1,
    kUnsupportedAls = 2,
    kAtlas = 3,
    kEve = 4,
    kNocturne = 5,
    kKohaku = 6,
    kMaxValue = kKohaku
  };

  static constexpr int kNumberDeviceClasses =
      static_cast<int>(DeviceClass::kMaxValue) + 1;

  // A histogram recorded in UMA, showing reasons why daily metrics are
  // reported.
  static constexpr char kDailyEventIntervalName[] =
      "AutoScreenBrightness.MetricsDailyEventInterval";

  // Histogram names of daily counts.
  static constexpr char kNoAlsUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.NoAls";
  static constexpr char kSupportedAlsUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.SupportedAls";
  static constexpr char kUnsupportedAlsUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.UnsupportedAls";
  static constexpr char kAtlasUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.Atlas";
  static constexpr char kEveUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.Eve";
  static constexpr char kNocturneUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.Nocturne";
  static constexpr char kKohakuUserAdjustmentName[] =
      "AutoScreenBrightness.DailyUserAdjustment.Kohaku";

  // Registers prefs used by MetricsReporter in |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // RegisterLocalStatePrefs() must be called before instantiating this class.
  MetricsReporter(chromeos::PowerManagerClient* power_manager_client,
                  PrefService* local_state_pref_service);

  MetricsReporter(const MetricsReporter&) = delete;
  MetricsReporter& operator=(const MetricsReporter&) = delete;

  ~MetricsReporter() override;

  // PowerManagerClient::Observer:
  void SuspendDone(base::TimeDelta duration) override;

  // Sets |device_class_|. Should only be called once after adapter is
  // initialized.
  void SetDeviceClass(DeviceClass device_class);

  // Increments number of adjustments for |device_class_|. Should only
  // be called after |SetDeviceClass| is called.
  void OnUserBrightnessChangeRequested();

  // Calls ReportDailyMetrics directly.
  void ReportDailyMetricsForTesting(metrics::DailyEvent::IntervalType type);

 private:
  class DailyEventObserver;

  // Called by DailyEventObserver whenever a day has elapsed according to
  // |daily_event_|.
  void ReportDailyMetrics(metrics::DailyEvent::IntervalType type);

  // Used as an index into |daily_counts_| for counting adjustments.
  // Set once and then never changed during the Chrome session.
  std::optional<DeviceClass> device_class_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_client_observation_{this};

  raw_ptr<PrefService> pref_service_;  // Not owned.

  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Instructs |daily_event_| to check if a day has passed.
  base::RepeatingTimer timer_;

  // Daily count for each DeviceClass. Ordered by DeviceClass values.
  // Initial values will be loaded from prefs service.
  std::array<int, kNumberDeviceClasses> daily_counts_;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_METRICS_REPORTER_H_
