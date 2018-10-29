// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "ash/system/message_center/arc/arc_notification_surface_manager.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/accessibility/ax_host_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/wm/public/activation_change_observer.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class AXTreeSourceArc;
class ArcBridgeService;

// ArcAccessibilityHelperBridge is an instance to receive converted Android
// accessibility events and info via mojo interface and dispatch them to chrome
// os components.
class ArcAccessibilityHelperBridge
    : public KeyedService,
      public mojom::AccessibilityHelperHost,
      public ConnectionObserver<mojom::AccessibilityHelperInstance>,
      public wm::ActivationChangeObserver,
      public AXTreeSourceArc::Delegate,
      public ArcAppListPrefs::Observer,
      public ash::ArcNotificationSurfaceManager::Observer {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAccessibilityHelperBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcAccessibilityHelperBridge(content::BrowserContext* browser_context,
                               ArcBridgeService* arc_bridge_service);
  ~ArcAccessibilityHelperBridge() override;

  // Sets ChromeVox or TalkBack active for the current task.
  void SetNativeChromeVoxArcSupport(bool enabled);

  // Receives the result of setting native ChromeVox Arc support.
  void OnSetNativeChromeVoxArcSupportProcessed(
      std::unique_ptr<aura::WindowTracker> window_tracker,
      bool enabled,
      bool processed);

  // KeyedService overrides.
  void Shutdown() override;

  // ConnectionObserver<mojom::AccessibilityHelperInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::AccessibilityHelperHost overrides.
  void OnAccessibilityEventDeprecated(
      mojom::AccessibilityEventType event_type,
      mojom::AccessibilityNodeInfoDataPtr event_source) override;
  void OnAccessibilityEvent(
      mojom::AccessibilityEventDataPtr event_data) override;
  void OnNotificationStateChanged(
      const std::string& notification_key,
      mojom::AccessibilityNotificationStateType state) override;

  // AXTreeSourceArc::Delegate overrides.
  void OnAction(const ui::AXActionData& data) const override;

  // ArcAppListPrefs::Observer overrides.
  void OnTaskDestroyed(int32_t task_id) override;

  // ArcNotificationSurfaceManager::Observer overrides.
  void OnNotificationSurfaceAdded(
      ash::ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(
      ash::ArcNotificationSurface* surface) override {}

  const std::map<int32_t, std::unique_ptr<AXTreeSourceArc>>&
  task_id_to_tree_for_test() const {
    return task_id_to_tree_;
  }

  const std::map<std::string, std::unique_ptr<AXTreeSourceArc>>&
  notification_key_to_tree_for_test() const {
    return notification_key_to_tree_;
  }

  void set_filter_type_all_for_test() { use_filter_type_all_for_test_ = true; }

 protected:
  virtual aura::Window* GetActiveWindow();

 private:
  // wm::ActivationChangeObserver overrides.
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  void OnActionResult(const ui::AXActionData& data, bool result) const;

  void OnAccessibilityStatusChanged(
      const chromeos::AccessibilityStatusEventDetails& event_details);
  arc::mojom::AccessibilityFilterType GetFilterTypeForProfile(Profile* profile);
  void UpdateFilterType();
  void UpdateWindowProperties(aura::Window* window);
  void SetExploreByTouchEnabled(bool enabled);
  void UpdateTreeIdOfNotificationSurface(const std::string& notification_key,
                                         ui::AXTreeID tree_id);

  AXTreeSourceArc* GetFromTaskId(int32_t task_id);
  AXTreeSourceArc* CreateFromTaskId(int32_t task_id);
  AXTreeSourceArc* GetFromNotificationKey(const std::string& notification_key);
  AXTreeSourceArc* CreateFromNotificationKey(
      const std::string& notification_key);
  AXTreeSourceArc* GetFromTreeId(ui::AXTreeID tree_id) const;

  bool activation_observer_added_ = false;
  Profile* const profile_;
  ArcBridgeService* const arc_bridge_service_;
  std::map<int32_t, std::unique_ptr<AXTreeSourceArc>> task_id_to_tree_;
  std::map<std::string, std::unique_ptr<AXTreeSourceArc>>
      notification_key_to_tree_;
  std::unique_ptr<chromeos::AccessibilityStatusSubscription>
      accessibility_status_subscription_;
  bool use_filter_type_all_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
