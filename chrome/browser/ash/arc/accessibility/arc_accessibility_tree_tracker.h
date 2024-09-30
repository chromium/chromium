// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_helper_instance_remote_proxy.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
class WindowTracker;
}  // namespace aura

namespace ash {
class ArcNotificationSurface;
}

namespace arc {

using SetNativeChromeVoxCallback = base::OnceCallback<void(
    extensions::api::accessibility_private::SetNativeChromeVoxResponse)>;

// ArcAccessibilityTreeTracker is responsible for mapping accessibility tree
// from android to exo window / surfaces.
class ArcAccessibilityTreeTracker : public aura::EnvObserver {
 public:
  enum class TreeKeyType {
    kTaskId,
    kNotificationKey,
    kInputMethod,
  };

  using TreeKey = std::tuple<TreeKeyType, int32_t, std::string>;
  using TreeMap =
      std::map<TreeKey, std::unique_ptr<ax::android::AXTreeSourceAndroid>>;

  ArcAccessibilityTreeTracker(
      ax::android::AXTreeSourceAndroid::Delegate* tree_source_delegate_,
      Profile* const profile,
      const AccessibilityHelperInstanceRemoteProxy&
          accessibility_helper_instance,
      ArcBridgeService* const arc_bridge_service);
  ~ArcAccessibilityTreeTracker() override;

  ArcAccessibilityTreeTracker(ArcAccessibilityTreeTracker&&) = delete;
  ArcAccessibilityTreeTracker& operator=(ArcAccessibilityTreeTracker&&) =
      delete;

  // aura::EnvObserver overrides:
  void OnWindowInitialized(aura::Window* window) override;

  void OnWindowFocused(aura::Window* gained_focus, aura::Window* lost_focus);

  void OnWindowDestroying(aura::Window* window);

  void Shutdown();

  // To be called when enabled accessibility features are changed.
  void OnEnabledFeatureChanged(
      ax::android::mojom::AccessibilityFilterType new_filter_type);

  // Request to send the tree with the specified AXTreeID.
  bool EnableTree(const ui::AXTreeID& tree_id);

  // Returns a pointer to the ax::android::AXTreeSourceAndroid corresponding to
  // the event source.
  ax::android::AXTreeSourceAndroid* OnAccessibilityEvent(
      const ax::android::mojom::AccessibilityEventData* const event_data);

  void OnNotificationSurfaceAdded(ash::ArcNotificationSurface* surface);

  void OnNotificationSurfaceRemoved(ash::ArcNotificationSurface* surface);

  void OnNotificationWindowRemoved(aura::Window* window);

  void OnNotificationStateChanged(
      const std::string& notification_key,
      const ax::android::mojom::AccessibilityNotificationStateType& state);

  void OnAndroidVirtualKeyboardVisibilityChanged(bool visible);

  // To be called via mojo from Android.
  void OnToggleNativeChromeVoxArcSupport(bool enabled);

  // To be called from chrome automation private API.
  void SetNativeChromeVoxArcSupport(bool enabled,
                                    SetNativeChromeVoxCallback callback);

  // Receives the result of setting native ChromeVox ARC support.
  void OnSetNativeChromeVoxArcSupportProcessed(
      std::unique_ptr<aura::WindowTracker> window_tracker,
      bool enabled,
      SetNativeChromeVoxCallback callback,
      ax::android::mojom::SetNativeChromeVoxResponse response);

  // Returns a tree source for the specified AXTreeID.
  ax::android::AXTreeSourceAndroid* GetFromTreeId(
      const ui::AXTreeID& tree_id) const;

  // Invalidates all trees (resets serializers).
  void InvalidateTrees();

  int GetTrackingArcWindowCount() const;

  bool IsArcFocused() const;

  const TreeMap& trees_for_test() const { return trees_; }

  bool is_native_chromevox_enabled() const { return native_chromevox_enabled_; }

 protected:
  // Start observing the given window.
  void TrackWindow(aura::Window* window);

  // Start observing the given window as a children of the toplevel ARC window.
  void TrackChildWindow(aura::Window* window);

 private:
  class FocusChangeObserver;
  class WindowsObserver;
  class ChildWindowsObserver;
  class ArcInputMethodManagerServiceObserver;
  class MojoConnectionObserver;
  class NotificationObserver;
  class UmaRecorder;

  ax::android::AXTreeSourceAndroid* GetFromKey(const TreeKey&);
  ax::android::AXTreeSourceAndroid* CreateFromKey(TreeKey,
                                                  aura::Window* window);

  // Updates task_id and window_id properties when properties of the toplevel
  // ARC++ window change.
  // As a side-effect, when a new task id is assigned to the window, it may
  // also trigger updating child window ids.
  void UpdateTopWindowIds(aura::Window* window);

  // Updates task_id and window_id propertied when properties of child ARC++
  // window change.
  void UpdateChildWindowIds(aura::Window* window);

  // Updates properties set to the given aura::Window.
  void UpdateWindowProperties(aura::Window* window);

  void StartTrackingWindows();

  void StartTrackingWindows(aura::Window* window);

  // Virtual for testing.
  virtual void DispatchCustomSpokenFeedbackToggled(bool enabled);
  virtual aura::Window* GetFocusedArcWindow() const;

  const raw_ptr<Profile> profile_;
  raw_ptr<ax::android::AXTreeSourceAndroid::Delegate> tree_source_delegate_;
  const raw_ref<const AccessibilityHelperInstanceRemoteProxy>
      accessibility_helper_instance_;

  TreeMap trees_;

  std::unique_ptr<FocusChangeObserver> focus_change_observer_;
  std::unique_ptr<WindowsObserver> windows_observer_;
  std::unique_ptr<ChildWindowsObserver> child_windows_observer_;
  std::unique_ptr<ArcInputMethodManagerServiceObserver>
      input_manager_service_observer_;
  std::unique_ptr<MojoConnectionObserver> connection_observer_;
  std::unique_ptr<NotificationObserver> notification_observer_;

  std::unique_ptr<UmaRecorder> uma_recorder_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};

  // a11y window id (obtained from exo, put for each window) to task id.
  std::map<int32_t, int32_t> window_id_to_task_id_;
  // task id to top aura::window.
  std::map<int32_t, raw_ptr<aura::Window, CtnExperimental>> task_id_to_window_;

  ax::android::mojom::AccessibilityFilterType filter_type_ =
      ax::android::mojom::AccessibilityFilterType::OFF;

  // Set of task id where TalkBack is enabled. ChromeOS native accessibility
  // support should be disabled for these tasks.
  std::set<int32_t> talkback_enabled_task_ids_;

  // True if native ChromeVox support is enabled. False if TalkBack is enabled.
  bool native_chromevox_enabled_ = true;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_
