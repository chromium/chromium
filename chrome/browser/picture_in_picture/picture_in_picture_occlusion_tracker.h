// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_TRACKER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_TRACKER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// The PictureInPictureOcclusionTracker keeps track of picture-in-picture
// widgets (both video picture-in-picture and document picture-in-picture) and
// notifies observers when their given widget has started or stopped being
// occluded by a picture-in-picture widget. This can be useful for
// security-sensitive widgets which may want to close/disable themselves when
// occluded.
//
// The simplest way to use the PictureInPictureOcclusionTracker is to use a
// ScopedPictureInPictureOcclusionObservation in a class that implements
// PictureInPictureOcclusionObserver:
//
// // Example where the widget itself observes occlusions:
// class FooWidget : public views::Widget,
//                   public PictureInPictureOcclusionObserver {
//  public:
//   FooWidget() {
//     occlusion_observation_.Observe(this);
//   }
//
//   void OnOcclusionStateChanged(bool occluded) {
//     // Only enable the Baz input field when we're not occluded.
//     SetBazEnabled(!occluded);
//   }
//
//  private:
//   ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
// };
//
// // Example where a separate class that owns the widget observes occlusions:
// class BarWidgetOwner : public PictureInPictureOcclusionObserver {
//  public:
//   ...
//
//   void OpenBarWidget() {
//     bar_widget_ = ...;
//     bar_widget_.Show();
//     occlusion_observation_.Observe(bar_widget_);
//   }
//
//   void OnOcclusionStateChanged(bool occluded) {
//     // Close the widget if it gets occluded. This will automatically stop
//     // observing it for occlusions.
//     if (occluded) {
//       CloseBarWidget();
//     }
//   }
//
//  private:
//   ScopedPictureInPictureOcclusionObservation occlusion_observation_{this};
// };
class PictureInPictureOcclusionTracker : public views::WidgetObserver {
 public:
  PictureInPictureOcclusionTracker();
  PictureInPictureOcclusionTracker(const PictureInPictureOcclusionTracker&) =
      delete;
  PictureInPictureOcclusionTracker& operator=(
      const PictureInPictureOcclusionTracker&) = delete;
  ~PictureInPictureOcclusionTracker() override;

  // Informs the PictureInPictureOcclusionTracker of a new picture-in-picture
  // widget that can potenitally occlude other widgets. The
  // PictureInPictureOcclusionTracker will automatically stop tracking it
  // when we receive an associated `OnWidgetDestroying()` call from the widget
  // itself.
  void OnPictureInPictureWidgetOpened(views::Widget* picture_in_picture_widget);

  // Start observing occlusion state changes for
  // `observer->occludable_widget()`.
  void AddObserver(PictureInPictureOcclusionTrackerObserver* observer);

  // Stop observing occlusion state changes.
  void RemoveObserver(PictureInPictureOcclusionTrackerObserver* observer);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget,
                                 bool visibility) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // Allows tests to check which picture-in-picture widgets are currently being
  // observed.
  std::vector<views::Widget*> GetPictureInPictureWidgetsForTesting();

  // Allows tests to manually set an occlusion state for an observed widget.
  void SetWidgetOcclusionStateForTesting(views::Widget* observed_widget,
                                         bool occluded);

 private:
  struct ObservedWidgetData {
    // True if the widget associated with this observation is a
    // picture-in-picture widget.
    bool is_picture_in_picture_widget = false;

    // True if the widget associated with this observation is occluded by a
    // picture-in-picture widget. Only set for widgets which have an associated
    // PictureInPictureOcclusionObserver (i.e. when `number_of_direct_observers`
    // is greater than zero).
    bool occluded = false;

    // The number of PictureInPictureOcclusionObservers directly observing the
    // occlusion state of this widget.
    int number_of_direct_observers = 0;

    // The number of PictureInPictureOcclusionObservers whose observation
    // depends on this widget, which means either:
    // 1) The PictureInPictureOcclusionObserver is directly observing the
    // occlusion state of this widget, OR
    // 2) The PictureInPictureOcclusionObserver is observing the occlusion state
    // of a child of this widget.
    int number_of_dependent_observers = 0;

    // If set and true, we treat the widget as occluded. If set and false, we
    // treat the widget as unoccluded. For testing only.
    std::optional<bool> forced_occlusion_state;
  };

  // Start observing this widget, plus any of its parent widgets.
  // `directly_observe_this_widget` is true if `widget` should be observed for
  // occlusion and false if we only observe `widget` due to its child widget
  // being observed for occlusion.
  void ObserveWidgetAndParents(views::Widget* widget,
                               bool directly_observe_this_widget = true);

  // The reverse operation of `ObserveWidgetAndParents()`.
  void UnobserveWidgetAndParents(views::Widget* widget,
                                 bool directly_unobserve_this_widget = true);

  // Calculates the occlusion state for all currently observed widgets and
  // informs the observers that have changed state.
  void UpdateAllObserverStates();

  // Calculates the occlusion state for the given widget and informs the
  // observers if it has changed state. When |force_update| is true, it will
  // inform observers even if the state has not changed.
  void UpdateObserverStateForWidget(views::Widget* widget,
                                    bool force_update = false);

  base::ObserverList<PictureInPictureOcclusionTrackerObserver> observers_;

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
  base::flat_map<views::Widget*, ObservedWidgetData> observed_widget_data_;

  // Used to ensure that frequent `OnWidgetBoundsChanged()` calls from dragging
  // a window don't calculate occlusion too often.
  base::OneShotTimer bounds_changed_throttle_timer_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_TRACKER_H_
