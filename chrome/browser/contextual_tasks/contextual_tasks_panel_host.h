// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_H_

#include <memory>

#include "base/observer_list_types.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// Interface for supporting view creation and UI interaction logic for the
// ContextualTasksPanelController. It is responsible for abstracting away
// platform-specific differences (e.g. bottom sheet on mobile vs. side panel on
// desktop).
class ContextualTasksPanelHost {
 public:
  // The visibility status of the contextual tasks panel.
  enum class SurfaceState {
    kVisible,  // Panel opened
    kClosed,   // Panel dismissed
  };

  // The origin of the event that caused a visibility change for the panel.
  enum class StateChangeReason {
    kUserAction,    // e.g. swipe down, close button
    kSystemAction,  // e.g. priority interruption
  };

  // The means by which the panel should be animated to open/close.
  enum class AnimationStyle {
    kStandard,           // Animate panel open/close from edge
    kTransitionFromTab,  // Animate panel from the active tab content's bounds
    kNoAnimation,        // Disable animation (e.g. when user closes tab)
  };

  // Observer to be notified of state changes on the panel UI (e.g. user-
  // initiated show/hide actions).
  class Observer : public base::CheckedObserver {
   public:
    // Notifies that the physical UI state changed.
    virtual void OnSurfaceStateChanged(SurfaceState state,
                                       StateChangeReason reason) = 0;
  };

  static std::unique_ptr<ContextualTasksPanelHost> Create(
      BrowserWindowInterface* browser_window);

  virtual ~ContextualTasksPanelHost() = default;

  // Register/unregister observers.
  virtual void AddObserver(Observer* observer) = 0;

  virtual void RemoveObserver(Observer* observer) = 0;

  // Visibility commands.
  // Show the panel.
  virtual void Show(AnimationStyle animation) = 0;

  // Close the panel. `kTransitionFromTab` is not supported for closing.
  virtual void Close(AnimationStyle animation) = 0;

  // State checks.
  // Return whether the panel is ready to display WebContents.
  virtual bool IsPanelInitialized() = 0;

  // Check if the panel is currently opening for Contextual Task as another
  // feature might also show panel.
  virtual bool IsPanelOpenForContextualTask() const = 0;

  // Whether panel should not be opened for Contextual Task given the current UI
  // state, e.g. on desktop if the glic side panel is open.
  virtual bool IsPanelSuppressed() const = 0;

  // For testing only. Sets whether the panel should be suppressed.
  virtual void SetPanelSuppressedForTesting(bool suppressed) = 0;

  // Content management.
  // Get current WebContents attached to panel, or nullptr if none attached.
  virtual content::WebContents* GetWebContents() = 0;

  // Configures the panel to display the provided WebContents.
  virtual void SetWebContents(content::WebContents* web_contents) = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_H_
