// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "ash/components/arc/session/connection_observer.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_helper_instance_remote_proxy.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_tree_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-forward.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
class EventRouter;
}

namespace gfx {
class Rect;
}  // namespace gfx

namespace ax::android {
class AXTreeSourceAndroid;
}

namespace arc {

class ArcBridgeService;

// ArcAccessibilityHelperBridge is an instance to receive converted Android
// accessibility events and info via mojo interface and dispatch them to Chrome
// OS components.
class ArcAccessibilityHelperBridge
    : public KeyedService,
      public ax::android::mojom::AccessibilityHelperHost,
      public ConnectionObserver<
          ax::android::mojom::AccessibilityHelperInstance>,
      public ax::android::AXTreeSourceAndroid::Delegate,
      public ash::ArcNotificationSurfaceManager::Observer,
      public extensions::AutomationEventRouterObserver {
 public:
  // Builds the ArcAccessibilityHelperBridgeFactory.
  static void CreateFactory();

  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAccessibilityHelperBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcAccessibilityHelperBridge(content::BrowserContext* browser_context,
                               ArcBridgeService* arc_bridge_service);

  ArcAccessibilityHelperBridge(const ArcAccessibilityHelperBridge&) = delete;
  ArcAccessibilityHelperBridge& operator=(const ArcAccessibilityHelperBridge&) =
      delete;

  ~ArcAccessibilityHelperBridge() override;

  // Sets ChromeVox or TalkBack active for the current task.
  void SetNativeChromeVoxArcSupport(bool enabled,
                                    SetNativeChromeVoxCallback callback);

  // Request Android to send the entire tree with the tree id. Returns true if
  // the specified tree is an ARC tree and a request was sent.
  bool EnableTree(const ui::AXTreeID& tree_id);

  // KeyedService overrides.
  void Shutdown() override;

  // ConnectionObserver<mojom::AccessibilityHelperInstance> overrides.
  void OnConnectionReady() override;

  // mojom::AccessibilityHelperHost overrides.
  void OnAccessibilityEvent(
      ax::android::mojom::AccessibilityEventDataPtr event_data) override;
  void OnNotificationStateChanged(
      const std::string& notification_key,
      ax::android::mojom::AccessibilityNotificationStateType state) override;
  void OnToggleNativeChromeVoxArcSupport(bool enabled) override;

  // ax::android::AXTreeSourceAndroid::Delegate overrides.
  void OnAction(const ui::AXActionData& data) const override;
  bool UseFullFocusMode() const override;

  // ArcNotificationSurfaceManager::Observer overrides.
  // TODO(hirokisato): Remove this method once refactoring finishes.
  // This exists only to do refactoring without large test change.
  void OnNotificationSurfaceAdded(
      ash::ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(
      ash::ArcNotificationSurface* surface) override {}

  // AutomationEventRouterObserver overrides.
  void AllAutomationExtensionsGone() override;
  void ExtensionListenerAdded() override;

  const ArcAccessibilityTreeTracker::TreeMap& trees_for_test() const {
    return tree_tracker_.trees_for_test();
  }

  static void EnsureFactoryBuilt();

 private:
  // virtual for testing.
  virtual extensions::EventRouter* GetEventRouter() const;
  virtual ax::android::mojom::AccessibilityFilterType GetFilterType();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  void OnActionResult(const ui::AXActionData& data, bool result) const;
  void OnGetTextLocationDataResult(
      const ui::AXActionData& data,
      const std::optional<gfx::Rect>& result_rect) const;

  void PopulateActionParameters(
      const ui::AXActionData& chrome_data,
      ax::android::mojom::AccessibilityActionData& action_data) const;

  std::optional<gfx::Rect> OnGetTextLocationDataResultInternal(
      const ui::AXTreeID& ax_tree_id,
      const std::optional<gfx::Rect>& result_rect) const;

  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& event_details);
  void UpdateEnabledFeature();
  void HandleFilterTypeFocusEvent(
      ax::android::mojom::AccessibilityEventDataPtr event_data);
  void HandleFilterTypeAllEvent(
      ax::android::mojom::AccessibilityEventDataPtr event_data);

  void DispatchEventTextAnnouncement(
      ax::android::mojom::AccessibilityEventData* event_data) const;

  bool is_focus_event_enabled_ = false;
  bool use_full_focus_mode_ = false;
  const raw_ptr<Profile> profile_;
  const raw_ptr<ArcBridgeService> arc_bridge_service_;

  const AccessibilityHelperInstanceRemoteProxy accessibility_helper_instance_;

  ArcAccessibilityTreeTracker tree_tracker_;

  base::CallbackListSubscription accessibility_status_subscription_;

  ax::android::mojom::AccessibilityFilterType filter_type_ =
      ax::android::mojom::AccessibilityFilterType::OFF;

  base::ScopedObservation<extensions::AutomationEventRouter,
                          extensions::AutomationEventRouterObserver>
      automation_event_router_observer_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
