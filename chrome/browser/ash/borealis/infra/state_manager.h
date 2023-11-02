// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_INFRA_STATE_MANAGER_H_
#define CHROME_BROWSER_ASH_BOREALIS_INFRA_STATE_MANAGER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/infra/expected.h"
#include "chrome/browser/ash/borealis/infra/transition.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace borealis {

// A tool for organizing callbacks that depend on the system being in a certain
// state, represented by an object of type |State|. Clients of the state manager
// can make requests that the system transition between the "On" and "Off"
// states, which relate to the presence or absence of the |State| instance.
//
// A request to transition the system comes in the form of a callback, which
// will be invoked asynchronously when the system is in the requested state. The
// system will transition based on the most recent callback.
//
// The state manager can itself be in one of 4 phases:
//
//        [Off] <-----> {Transitioning On}
//          ^                   |
//          |                   v
//  {Transitioning Off} <------ [On]
//
// These phases relate to |State| instances as well as the enqueueing of
// callbacks:
//  - |State| is nonexistent while "Off", is immediately created at the start of
//    "Transitioning On", exists while "On", and is immediately deleted at the
//    start of "Transitioning Off"
//  - When the system is "Off", a callback to turn the system "On" will be
//    enqueued, and move the manger to "Transitioning On". Additional requests
//    will be enqueued until the system is "On", when all queued callbacks will
//    be invoked (as will new requests). Vice-versa is true for turning the
//    system "Off" when in the "On" state.
//  - When a system is "Transitioning On", all requests to turn the system "Off"
//    will be rejected, until the system is "On", then they will be enqueued.
//    Vice-versa is true for turning the system "On" in the "Transitioning Off"
//    state.
//
// A system can only fail to turn "On", in which case the |State| instance will
// be deleted and the failure |OnError| will be reported to callbacks. A system
// can always transition to "Off" but it may do so uncleanly, which will result
// in an |OffError| being reported.
template <typename State, typename OnError, typename OffError>
class BorealisStateManager {
 public:
  // An empty state which is a marker that the state manager is off. This state
  // holds no data,
  struct OffState {};

  // Convenience typedefs.
  using OnTransition = Transition<OffState, State, OnError>;
  using OffTransition = Transition<State, OffState, OffError>;
  using WhenOn = void(Expected<State*, OnError>);
  using WhenOff = void(absl::optional<OffError>);

  // Create the state object, turning the state to "on". The |callback| will be
  // invoked on completion with the result.
  void TurnOn(base::OnceCallback<WhenOn> callback) {
    switch (GetPhase()) {
      case Phase::kOff:
        on_transition_ = GetOnTransition();
        on_transition_->Begin(std::make_unique<OffState>(),
                              base::BindOnce(&BorealisStateManager::CompleteOn,
                                             weak_ptr_factory_.GetWeakPtr()));
        pending_on_callbacks_.AddUnsafe(std::move(callback));
        break;
      case Phase::kTransitioningOn:
        pending_on_callbacks_.AddUnsafe(std::move(callback));
        break;
      case Phase::kOn:
        std::move(callback).Run(Expected<State*, OnError>(instance_.get()));
        break;
      case Phase::kTransitioningOff:
        std::move(callback).Run(
            Unexpected<State*, OnError>(GetIsTurningOffError()));
        break;
    }
  }

  // Remove the state object, turning the state to "off". The |callback| will be
  // invoked on completion with an error (or not, if successful).
  void TurnOff(base::OnceCallback<WhenOff> callback) {
    switch (GetPhase()) {
      case Phase::kOff:
        std::move(callback).Run(absl::nullopt);
        break;
      case Phase::kTransitioningOn:
        std::move(callback).Run(GetIsTurningOnError());
        break;
      case Phase::kOn:
        off_transition_ = GetOffTransition();
        off_transition_->Begin(
            std::move(instance_),
            base::BindOnce(&BorealisStateManager::CompleteOff,
                           weak_ptr_factory_.GetWeakPtr()));
        pending_off_callbacks_.AddUnsafe(std::move(callback));
        break;
      case Phase::kTransitioningOff:
        pending_off_callbacks_.AddUnsafe(std::move(callback));
        break;
    }
  }

 protected:
  // Factory method to build the transition needed to turn this state manager
  // on.
  virtual std::unique_ptr<OnTransition> GetOnTransition() = 0;

  // Factory method to build the transition needed to turn this state manager
  // off.
  virtual std::unique_ptr<OffTransition> GetOffTransition() = 0;

  // Factory method to build the error which was the result of rejecting a
  // request to turn the state on.
  virtual OnError GetIsTurningOffError() = 0;

  // Factory method to build the error which was the result of rejecting a
  // request to turn the state off.
  virtual OffError GetIsTurningOnError() = 0;

 private:
  // The phases of managing a state object.
  enum class Phase {
    kOff,
    kTransitioningOn,
    kOn,
    kTransitioningOff,
  };

  // Returns the current phase of the state manager.
  Phase GetPhase() const {
    if (instance_) {
      return Phase::kOn;
    } else if (on_transition_) {
      DCHECK(!off_transition_);
      return Phase::kTransitioningOn;
    } else if (off_transition_) {
      return Phase::kTransitioningOff;
    } else {
      return Phase::kOff;
    }
  }

  void CompleteOn(typename OnTransition::Result on_result) {
    if (on_result) {
      std::swap(instance_, on_result.Value());
      on_transition_.reset();
      pending_on_callbacks_.Notify(Expected<State*, OnError>(instance_.get()));
    } else {
      on_transition_.reset();
      pending_on_callbacks_.Notify(
          Unexpected<State*, OnError>(on_result.Error()));
    }
  }

  void CompleteOff(typename OffTransition::Result off_result) {
    off_transition_.reset();
    pending_off_callbacks_.Notify(
        off_result ? absl::nullopt
                   : absl::optional<OffError>(off_result.Error()));
  }

  std::unique_ptr<State> instance_;
  std::unique_ptr<OnTransition> on_transition_;
  std::unique_ptr<OffTransition> off_transition_;
  base::OnceCallbackList<WhenOn> pending_on_callbacks_;
  base::OnceCallbackList<WhenOff> pending_off_callbacks_;
  base::WeakPtrFactory<BorealisStateManager<State, OnError, OffError>>
      weak_ptr_factory_{this};
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_INFRA_STATE_MANAGER_H_
