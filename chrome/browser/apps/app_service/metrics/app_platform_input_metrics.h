// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_INPUT_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_INPUT_METRICS_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/apps/app_service/metrics/browser_to_tab_list.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/events/event_handler.h"

namespace apps {

class InstanceUpdate;

// This is used for logging, so do not remove or reorder existing entries.
enum class InputEventSource {
  kUnknown = 0,
  kMouse = 1,
  kStylus = 2,
  kTouch = 3,
  kKeyboard = 4,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kKeyboard,
};

extern const char kAppInputEventsKey[];

// This class is used to record the input events for the app windows.
class AppPlatformInputMetrics : public ui::EventHandler,
                                public InstanceRegistry::Observer,
                                public ukm::UkmRecorder::Observer {
 public:
  // For web apps and Chrome apps, there might be different app type name for
  // opening in tab or window. So record the app type name for the event count.
  using CountPerAppType = base::flat_map<AppTypeName, int>;

  // The map to record the event count for each InputEventSource.
  using EventSourceToCounts = base::flat_map<InputEventSource, CountPerAppType>;

  AppPlatformInputMetrics(Profile* profile,
                          const apps::AppRegistryCache& app_registry_cache,
                          InstanceRegistry& instance_registry);

  AppPlatformInputMetrics(const AppPlatformInputMetrics&) = delete;
  AppPlatformInputMetrics& operator=(const AppPlatformInputMetrics&) = delete;

  ~AppPlatformInputMetrics() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  void OnFiveMinutes();

  // Records the input events AppKM each 2 hours.
  void OnTwoHours();

 private:
  struct AppInfo {
    std::string app_id;
    AppTypeName app_type_name;
  };

  // InstanceRegistry::Observer:
  void OnInstanceUpdate(const InstanceUpdate& update) override;
  void OnInstanceRegistryWillBeDestroyed(InstanceRegistry* cache) override;

  // ukm::UkmRecorder::Observer:
  // Called only in Managed Guest Session since the observation is started only
  // in Managed Guest Session.
  void OnStartingShutdown() override;

  void SetAppInfoForActivatedWindow(AppType app_type,
                                    const std::string& app_id,
                                    aura::Window* window,
                                    const base::UnguessableToken& instance_id);

  void SetAppInfoForInactivatedWindow(
      const base::UnguessableToken& instance_id);

  void RecordEventCount(InputEventSource event_source,
                        ui::EventTarget* event_target);

  ukm::SourceId GetSourceId(const std::string& app_id);

  void RecordInputEventsAppKM();

  void RecordInputEventsAppKMForApp(const std::string& app_id,
                                    const EventSourceToCounts& event_counts);

  // Saves the input events in `app_id_to_event_count_per_two_hours_` to the
  // user pref each 2 hours. For example:
  // web_app_id1: {
  //   mouse:    { ChromeBrowser: 5, WebApp: 2}
  //   keyboard: { ChromeBrowser: 2, WebApp: 3}
  // },
  // chrome_app_id2: {
  //   stylus:   { ChromeBrowser: 2, ChromeApp: 12}
  //   keyboard: { ChromeBrowser: 3, ChromeApp: 30}
  // },
  // Arc_app_id3: {
  //   mouse:   { Arc: 5}
  // },
  void SaveInputEvents();

  // Records the input events AppKM saved in the user pref.
  void RecordInputEventsAppKMFromPref();

  // Returns true if recording is allowed for this app.
  bool ShouldRecordAppKMForApp(const std::string& app_id);

  raw_ptr<Profile> profile_;

  const raw_ref<const AppRegistryCache> app_registry_cache_;

  BrowserToTabList browser_to_tab_list_;

  bool should_record_ukm_from_pref_ = true;

  // The map from the window to the app info.
  base::flat_map<aura::Window*, AppInfo> window_to_app_info_;

  // Records the input even count for each app id in the past five minutes. Each
  // app id might have multiple events. For web apps and Chrome apps, there
  // might be different app type name, e.g. Chrome browser for apps opening in
  // a tab, or Web app for apps opening in a window. For example:
  // web_app_id1: {
  //   mouse:    { Chrome browser: 5, Web app: 2}
  //   Keyboard: { Chrome browser: 2, Web app: 3}
  // },
  // chrome_app_id2: {
  //   stylus:   { Chrome browser: 2, Chrome app: 12}
  //   Keyboard: { Chrome browser: 3, Chrome app: 30}
  // },
  // Arc_app_id3: {
  //   mouse:   { Arc: 5}
  // },
  std::map<std::string, EventSourceToCounts>
      app_id_to_event_count_per_two_hours_;

  base::ScopedObservation<InstanceRegistry, InstanceRegistry::Observer>
      instance_registry_observation_{this};

  // Observes `UkmRecorder` only in Managed Guest Session.
  base::ScopedObservation<ukm::UkmRecorder, ukm::UkmRecorder::Observer>
      ukm_recorder_observer_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_APP_PLATFORM_INPUT_METRICS_H_
