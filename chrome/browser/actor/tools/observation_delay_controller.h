// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/observation_delay_type.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

// Observes a page during tool-use and determines when the page has settled
// after an action and is ready for for an observation.
//
// When using the kWatchForLoad type, this class will watch for any document
// loads in the web contents. When the tool completes, this class delays until
// the load also finishes and then a new frame is generated and presented.
class ObservationDelayController : public content::WebContentsObserver {
 public:
  using ReadyCallback = base::OnceClosure;
  ObservationDelayController(content::RenderFrameHost& target_frame,
                             ObservationDelayType type);
  ~ObservationDelayController() override;

  // Note: Callback will always be executed asynchronously. It may be run after
  // this object is deleted so must manage its own lifetime.
  void Wait(ReadyCallback callback);

  // content::WebContentsObserver
  void DidStartLoading() override;
  void DidStopLoading() override;

 private:
  void WaitForVisualStateUpdate();
  void VisualStateUpdated(bool success);
  void Timeout();

  // Set only if using kWatchForLoad type.
  struct LoadWatcher {
    LoadWatcher();
    ~LoadWatcher();
    LoadWatcher(const LoadWatcher&) = delete;
    LoadWatcher& operator=(const LoadWatcher&) = delete;

    enum class State {
      kWaitingForLoadStart,
      kWaitingForLoadStop,
      kWaitingForVisualUpdate,
      kDone
    } state = State::kWaitingForLoadStart;

    ReadyCallback ready_callback_;
  };
  std::optional<LoadWatcher> load_watcher_;

  ObservationDelayType observation_type_;

  base::WeakPtrFactory<ObservationDelayController> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OBSERVATION_DELAY_CONTROLLER_H_
