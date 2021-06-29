// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>

#include "chrome/browser/ash/arc/accessibility/accessibility_helper_instance_remote_proxy.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
class WindowTracker;
}  // namespace aura

namespace arc {

class ArcAccessibilityHelperBridge;

// ArcAccessibilityTreeTracker is responsible for mapping accessibility tree
// from android to exo window / surfaces.
class ArcAccessibilityTreeTracker {
 public:
  enum class TreeKeyType {
    kTaskId,
    kNotificationKey,
    kInputMethod,
  };

  using TreeKey = std::tuple<TreeKeyType, int32_t, std::string>;
  using TreeMap = std::map<TreeKey, std::unique_ptr<AXTreeSourceArc>>;

  static TreeKey KeyForInputMethod();
  static TreeKey KeyForNotification(std::string notification_key);
  static TreeKey KeyForTaskId(int32_t value);

  ArcAccessibilityTreeTracker(ArcAccessibilityHelperBridge* owner,
                              Profile* const profile,
                              const AccessibilityHelperInstanceRemoteProxy&
                                  accessibility_helper_instance);
  ~ArcAccessibilityTreeTracker();

  ArcAccessibilityTreeTracker(ArcAccessibilityTreeTracker&&) = delete;
  ArcAccessibilityTreeTracker& operator=(ArcAccessibilityTreeTracker&&) =
      delete;

  void OnWindowFocused(aura::Window* gained_focus, aura::Window* lost_focus);

  void OnTaskDestroyed(int32_t task_id);

  void Shutdown();

  // To be called when enabled accessibility features are changed.
  void OnEnabledFeatureChanged(
      arc::mojom::AccessibilityFilterType new_filter_type);

  // To be called via chrome automation enableTree.
  bool RefreshTreeIfInActiveWindow(const ui::AXTreeID& tree_id);

  // Returns a pointer to the AXTreeSourceArc corresponding to the event
  // source.
  AXTreeSourceArc* OnAccessibilityEvent(
      const mojom::AccessibilityEventData* const event_data);

  // To be called via mojo from Android.
  void OnToggleNativeChromeVoxArcSupport(bool enabled);

  // To be called from chrome automation private API.
  void SetNativeChromeVoxArcSupport(bool enabled);

  // Receives the result of setting native ChromeVox ARC support.
  void OnSetNativeChromeVoxArcSupportProcessed(
      std::unique_ptr<aura::WindowTracker> window_tracker,
      bool enabled,
      bool processed);

  bool Erase(const TreeKey& key);

  // Returns a tree source for the specified AXTreeID.
  AXTreeSourceArc* GetFromTreeId(ui::AXTreeID tree_id) const;

  AXTreeSourceArc* GetFromKey(const TreeKey&);
  AXTreeSourceArc* CreateFromKey(TreeKey, AXTreeSourceArc::Delegate*);

  const TreeMap& trees_for_test() const { return trees_; }

 private:
  class FocusChangeObserver;
  class WindowObserver;
  class AppListPrefsObserver;

  // Update |window_id_to_task_id_| with a given window if necessary.
  void UpdateWindowIdMapping(aura::Window* window);

  void UpdateWindowProperties(aura::Window* window);

  ArcAccessibilityHelperBridge* owner_;
  const AccessibilityHelperInstanceRemoteProxy& accessibility_helper_instance_;

  TreeMap trees_;

  std::unique_ptr<FocusChangeObserver> focus_change_observer_;
  std::unique_ptr<WindowObserver> window_observer_;
  std::unique_ptr<AppListPrefsObserver> app_list_prefs_observer_;

  std::map<int32_t, int32_t> window_id_to_task_id_;

  // Set of task id where TalkBack is enabled. ChromeOS native accessibility
  // support should be disabled for these tasks.
  std::set<int32_t> talkback_enabled_task_ids_;

  // True if native ChromeVox support is enabled. False if TalkBack is enabled.
  bool native_chromevox_enabled_ = true;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_
