// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_INFRA_TRANSITION_H_
#define CHROME_BROWSER_ASH_BOREALIS_INFRA_TRANSITION_H_

#include <memory>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"

// TODO(b/172501195): Make these available outside namespace borealis.
namespace borealis {

// A transition object is used to represent a transformation of a start state
// |S| into a terminating state |T|. The transition can fail, in which case the
// result is an error state |E|.
//
// The transition centers around a state object of type |S|, which will become
// an instance of |T| if the transition succeeds, and an error |E| if it does
// not.
//
// The transition itself is in one of three modes:
//  - A "pending" transition has been created and is waiting for control of the
//    |S| instance to begin the transition.
//  - A "working" transition has control of the |S| object, and is transforming
//    it into |T|. There is no guarantee at this point about the existence of an
//    |S| or |T| instance.
//  - A "final" transition has completed, and either succeeded (in which case
//    a |T| instance exists) or failed (in which case |T| does not exist and an
//    |E| error has been generated.
//
// Once the transition is "final" no further work is possible and the transition
// can be deleted.
template <typename S, typename T, typename E>
class Transition {
 public:
  using InitialState = S;

  using FinalState = T;

  using ErrorState = E;

  using Result = base::expected<std::unique_ptr<T>, E>;

  using OnCompleteSignature = void(Result);

  using OnCompleteCallback = base::OnceCallback<OnCompleteSignature>;

  Transition() = default;
  virtual ~Transition() = default;

  // Begin the transition, marking this transition as "working" and giving
  // ownership of the |S| object to the transition.
  void Begin(std::unique_ptr<S> start_instance, OnCompleteCallback callback) {
    callback_ = std::move(callback);
    Start(std::move(start_instance));
  }

 protected:
  // Override this method to define the work that this transition performs. As
  // soon as this function is entered, the transition is "working", until you
  // call Succeed() or Fail(), which should be the last things you call.
  virtual void Start(std::unique_ptr<S> start_instance) = 0;

  // Called when the transition has completed successfully. This should be the
  // last thing you do.
  void Succeed(std::unique_ptr<T> terminating_instance) {
    if (!callback_) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  Result(std::move(terminating_instance))));
  }

  // Called when the transition has completed unsuccessfully. This should only
  // be called at the very end of the failing transition (including cleanup if
  // needed).
  void Fail(E error) {
    if (!callback_) {
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  base::unexpected(std::move(error))));
  }

 private:
  OnCompleteCallback callback_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_INFRA_TRANSITION_H_
