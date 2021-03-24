// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "ash/public/cpp/external_arc/message_center/arc_notification_surface_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/browser/ash/arc/input_method_manager/arc_input_method_manager_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/arc/mojom/accessibility_helper.mojom-forward.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace arc {

class AXTreeSourceArc;
class ArcBridgeService;

arc::mojom::CaptionStylePtr GetCaptionStyleFromPrefs(PrefService* prefs);

// ArcAccessibilityHelperBridge is an instance to receive converted Android
// accessibility events and info via mojo interface and dispatch them to Chrome
// OS components.
class ArcAccessibilityHelperBridge
    : public KeyedService,
      public mojom::AccessibilityHelperHost,
      public ConnectionObserver<mojom::AccessibilityHelperInstance>,
      public aura::client::FocusChangeObserver,
      public AXTreeSourceArc::Delegate,
      public ArcAppListPrefs::Observer,
      public arc::ArcInputMethodManagerService::Observer,
      public ash::ArcNotificationSurfaceManager::Observer,
      public aura::WindowObserver {
 public:
  // Builds the ArcAccessibilityHelperBridgeFactory.
  static void CreateFactory();

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

  // Request Android to send the entire tree with the tree id. Returns true if
  // the specified tree exists in ARC and a request was sent.
  bool RefreshTreeIfInActiveWindow(const ui::AXTreeID& tree_id);

  // KeyedService overrides.
  void Shutdown() override;

  // ConnectionObserver<mojom::AccessibilityHelperInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::AccessibilityHelperHost overrides.
  void OnAccessibilityEvent(
      mojom::AccessibilityEventDataPtr event_data) override;
  void OnNotificationStateChanged(
      const std::string& notification_key,
      mojom::AccessibilityNotificationStateType state) override;
  void OnToggleNativeChromeVoxArcSupport(bool enabled) override;

  // AXTreeSourceArc::Delegate overrides.
  void OnAction(const ui::AXActionData& data) const override;
  bool UseFullFocusMode() const override;

  // ArcAppListPrefs::Observer overrides.
  void OnTaskDestroyed(int32_t task_id) override;

  // ArcInputMethodManagerService::Observer overrides.
  void OnAndroidVirtualKeyboardVisibilityChanged(bool visible) override;

  // ArcNotificationSurfaceManager::Observer overrides.
  void OnNotificationSurfaceAdded(
      ash::ArcNotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(
      ash::ArcNotificationSurface* surface) override {}

  // aura::client::FocusChangeObserver overrides.
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // aura::WindowObserver overrides.
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  void InvokeUpdateEnabledFeatureForTesting();

  enum class TreeKeyType {
    kTaskId,
    kNotificationKey,
    kInputMethod,
  };

  using TreeKey = std::tuple<TreeKeyType, int32_t, std::string>;
  using TreeMap = std::map<TreeKey, std::unique_ptr<AXTreeSourceArc>>;

  static TreeKey KeyForNotification(std::string notification_key);

  const TreeMap& trees_for_test() const { return trees_; }

 private:
  // virtual for testing.
  virtual aura::Window* GetFocusedArcWindow() const;
  virtual extensions::EventRouter* GetEventRouter() const;
  virtual arc::mojom::AccessibilityFilterType GetFilterTypeForProfile(
      Profile* profile);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  void UpdateCaptionSettings() const;

  void OnActionResult(const ui::AXActionData& data, bool result) const;
  void OnGetTextLocationDataResult(
      const ui::AXActionData& data,
      const base::Optional<gfx::Rect>& result_rect) const;

  base::Optional<gfx::Rect> OnGetTextLocationDataResultInternal(
      const base::Optional<gfx::Rect>& result_rect) const;

  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& event_details);
  void UpdateEnabledFeature();
  void UpdateWindowProperties(aura::Window* window);
  void SetExploreByTouchEnabled(bool enabled);
  void UpdateTreeIdOfNotificationSurface(const std::string& notification_key,
                                         ui::AXTreeID tree_id);
  void HandleFilterTypeFocusEvent(mojom::AccessibilityEventDataPtr event_data);
  void HandleFilterTypeAllEvent(mojom::AccessibilityEventDataPtr event_data);

  // Update |window_id_to_task_id_| with a given window if necessary.
  void UpdateWindowIdMapping(aura::Window* window);

  void DispatchEventTextAnnouncement(
      mojom::AccessibilityEventData* event_data) const;
  void DispatchCustomSpokenFeedbackToggled(bool enabled) const;

  AXTreeSourceArc* CreateFromKey(TreeKey);
  AXTreeSourceArc* GetFromKey(const TreeKey&);
  AXTreeSourceArc* GetFromTreeId(ui::AXTreeID tree_id) const;

  bool focus_observer_added_ = false;
  bool is_focus_event_enabled_ = false;
  bool use_full_focus_mode_ = false;
  Profile* const profile_;
  ArcBridgeService* const arc_bridge_service_;
  TreeMap trees_;

  std::map<int32_t, int32_t> window_id_to_task_id_;

  base::CallbackListSubscription accessibility_status_subscription_;

  arc::mojom::AccessibilityFilterType filter_type_ =
      arc::mojom::AccessibilityFilterType::OFF;

  // Set of task id where TalkBack is enabled. ChromeOS native accessibility
  // support should be disabled for these tasks.
  std::set<int32_t> talkback_enabled_task_ids_;
  // True if native ChromeVox support is enabled.
  bool native_chromevox_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
