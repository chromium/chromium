// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_load_tracker_test_support.h"

#include "base/run_loop.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

enum class WaitForEvent { NO_LONGER_TRACKED };

class WaitForLoadingStateHelper : public TabLoadTracker::Observer {
 public:
  // Configures this helper to wait until the tab reaches the provided loading
  // state.
  WaitForLoadingStateHelper(content::WebContents* waiting_for_contents,
                            LoadingState waiting_for_state)
      : waiting_for_contents_(waiting_for_contents),
        waiting_for_state_(waiting_for_state),
        waiting_for_no_longer_tracked_(false),
        wait_successful_(false) {}

  // Configures this helper to wait until the tab is no longer tracked.
  WaitForLoadingStateHelper(content::WebContents* waiting_for_contents,
                            WaitForEvent no_longer_tracked_unused)
      : waiting_for_contents_(waiting_for_contents),
        waiting_for_state_(LoadingState::UNLOADED),
        waiting_for_no_longer_tracked_(true),
        wait_successful_(false) {}

#if !defined(OS_ANDROID)
  // Configures this helper to wait until all tabs in |tab_strip_model| are have
  // transitionned to |state|.
  WaitForLoadingStateHelper(TabStripModel* waiting_for_tab_strip,
                            LoadingState waiting_for_state)
      : waiting_for_tab_strip_(waiting_for_tab_strip),
        waiting_for_state_(waiting_for_state),
        waiting_for_no_longer_tracked_(false),
        wait_successful_(false) {}
#endif  // !defined(OS_ANDROID)

  ~WaitForLoadingStateHelper() override = default;

  bool Wait() {
    wait_successful_ = false;
    auto* tracker = resource_coordinator::TabLoadTracker::Get();

    // Early exit if the contents is already in the desired state.
    if (!waiting_for_no_longer_tracked_ && AllContentsReachedState()) {
      wait_successful_ = true;
      return wait_successful_;
    }

    tracker->AddObserver(this);
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    tracker->RemoveObserver(this);

    return wait_successful_;
  }

 protected:
  bool AllContentsReachedState() const {
    DCHECK(!waiting_for_no_longer_tracked_);

    auto* tracker = resource_coordinator::TabLoadTracker::Get();

    if (waiting_for_contents_) {
      return tracker->GetLoadingState(waiting_for_contents_) ==
             waiting_for_state_;
    }

#if defined(OS_ANDROID)
    return false;
#else
    DCHECK(waiting_for_tab_strip_);
    for (int i = 0; i < waiting_for_tab_strip_->count(); ++i) {
      if (tracker->GetLoadingState(waiting_for_tab_strip_->GetWebContentsAt(
              i)) != waiting_for_state_) {
        return false;
      }
    }

    return true;
#endif  // defined(OS_ANDROID)
  }

  void OnLoadingStateChange(content::WebContents* web_contents,
                            LoadingState old_loading_state,
                            LoadingState new_loading_state) override {
    if (waiting_for_no_longer_tracked_)
      return;
    if (AllContentsReachedState()) {
      wait_successful_ = true;
      run_loop_quit_closure_.Run();
    }
  }

  void OnStopTracking(content::WebContents* web_contents,
                      LoadingState loading_state) override {
    if (waiting_for_contents_ != web_contents)
      return;
    if (waiting_for_no_longer_tracked_) {
      wait_successful_ = true;
    } else if (waiting_for_contents_) {
      wait_successful_ = (waiting_for_state_ == loading_state);
    } else {
#if defined(OS_ANDROID)
      NOTREACHED();
#else
      DCHECK(waiting_for_tab_strip_);
      wait_successful_ = AllContentsReachedState();
#endif  // defined(OS_ANDROID)
    }
    run_loop_quit_closure_.Run();
  }

 private:
  // The WebContents or TabStripModel and state that is being waited for.
  content::WebContents* const waiting_for_contents_ = nullptr;
#if !defined(OS_ANDROID)
  TabStripModel* const waiting_for_tab_strip_ = nullptr;
#endif
  const LoadingState waiting_for_state_;
  const bool waiting_for_no_longer_tracked_;

  // Returns true if the wait was successful. This can be false if the contents
  // stops being tracked (is destroyed) before encountering the desired state.
  bool wait_successful_;

  base::Closure run_loop_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(WaitForLoadingStateHelper);
};

}  // namespace

bool WaitForTransitionToLoadingState(content::WebContents* contents,
                                     LoadingState loading_state) {
  WaitForLoadingStateHelper waiter(contents, loading_state);
  return waiter.Wait();
}

bool WaitForTransitionToUnloaded(content::WebContents* contents) {
  return WaitForTransitionToLoadingState(contents, LoadingState::UNLOADED);
}

bool WaitForTransitionToLoading(content::WebContents* contents) {
  return WaitForTransitionToLoadingState(contents, LoadingState::LOADING);
}

bool WaitForTransitionToLoaded(content::WebContents* contents) {
  return WaitForTransitionToLoadingState(contents, LoadingState::LOADED);
}

bool WaitUntilNoLongerTracked(content::WebContents* contents) {
  WaitForLoadingStateHelper waiter(contents, WaitForEvent::NO_LONGER_TRACKED);
  return waiter.Wait();
}

#if !defined(OS_ANDROID)
bool WaitForTransitionToLoadingState(TabStripModel* tab_strip,
                                     LoadingState loading_state) {
  WaitForLoadingStateHelper waiter(tab_strip, loading_state);
  return waiter.Wait();
}

bool WaitForTransitionToUnloaded(TabStripModel* tab_strip) {
  return WaitForTransitionToLoadingState(tab_strip, LoadingState::UNLOADED);
}

bool WaitForTransitionToLoading(TabStripModel* tab_strip) {
  return WaitForTransitionToLoadingState(tab_strip, LoadingState::LOADING);
}

bool WaitForTransitionToLoaded(TabStripModel* tab_strip) {
  return WaitForTransitionToLoadingState(tab_strip, LoadingState::LOADED);
}
#endif  // !defined(OS_ANDROID)

}  // namespace resource_coordinator
