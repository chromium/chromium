// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/observer_list_types.h"
#include "chrome/browser/ui/webui/side_panel/performance_controls/performance.mojom.h"

class ChromeBrowserMainExtraPartsPerformanceManager;

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager::user_tuning {

class PerformanceDetectionManager {
 public:
  using ResourceTypeSet =
      base::EnumSet<side_panel::mojom::ResourceType,
                    side_panel::mojom::ResourceType::kMinValue,
                    side_panel::mojom::ResourceType::kMaxValue>;

  class StatusObserver : public base::CheckedObserver {
   public:
    // Called immediately with the current status when AddStatusObserver is
    // called, then again on changes (frequency determined by the backend).
    // RequestStatus() requests an OOB update with most recent status.
    virtual void OnStatusChanged(side_panel::mojom::ResourceType resource_type,
                                 side_panel::mojom::HealthLevel health_level,
                                 bool actionable) {}
  };

  class ActionableTabsObserver : public base::CheckedObserver {
   public:
    // Called immediately with the current status when AddTabListObserver is
    // called, then again on changes (frequency determined by the backend).
    // RequestTabList() requests an OOB update with most recent status.
    virtual void OnActionableTabListChanged(
        side_panel::mojom::ResourceType resource_type,
        std::vector<content::WebContents*> tabs) {}
  };

  void AddStatusObserver(ResourceTypeSet resource_types, StatusObserver* o);
  void RemoveStatusObserver(ResourceTypeSet resource_types, StatusObserver* o);
  void RequestStatus(ResourceTypeSet resource_types, StatusObserver* o);

  void AddActionableTabsObserver(ResourceTypeSet resource_types,
                                 ActionableTabsObserver* o);
  void RemoveActionableTabsObserver(ResourceTypeSet resource_types,
                                    ActionableTabsObserver* o);
  void RequestActionableTabs(ResourceTypeSet resource_types,
                             ActionableTabsObserver* o);

  // Returns whether a PerformanceDetectionManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static PerformanceDetectionManager* GetInstance();

  ~PerformanceDetectionManager();

 private:
  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class PerformanceDetectionManagerTest;

  PerformanceDetectionManager();

  void Start();
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_
