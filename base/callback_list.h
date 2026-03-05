// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CALLBACK_LIST_H_
#define BASE_CALLBACK_LIST_H_

#include <algorithm>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/base_export.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

// OVERVIEW:
//
// A container for a list of callbacks. Provides callers the ability to manually
// or automatically unregister callbacks at any time, including during callback
// notification.
//
// TYPICAL USAGE:
//
// class MyWidget {
//  public:
//   using CallbackList = base::RepeatingCallbackList<void(const Foo&)>;
//
//   // Registers |cb| to be called whenever NotifyFoo() is executed.
//   CallbackListSubscription RegisterCallback(CallbackList::CallbackType cb) {
//     return callback_list_.Add(std::move(cb));
//   }
//
//  private:
//   // Calls all registered callbacks, with |foo| as the supplied arg.
//   void NotifyFoo(const Foo& foo) {
//     callback_list_.Notify(foo);
//   }
//
//   CallbackList callback_list_;
// };
//
//
// class MyWidgetListener {
//  private:
//   void OnFoo(const Foo& foo) {
//     // Called whenever MyWidget::NotifyFoo() is executed, unless
//     // |foo_subscription_| has been destroyed.
//   }
//
//   // Automatically deregisters the callback when deleted (e.g. in
//   // ~MyWidgetListener()).  Unretained(this) is safe here since
//   // `foo_subscription_` is a member of `this`.
//   CallbackListSubscription foo_subscription_ =
//       MyWidget::Get()->RegisterCallback(
//           base::BindRepeating(&MyWidgetListener::OnFoo,
//                               base::Unretained(this)));
// };
//
// UNSUPPORTED:
//
// * Destroying or clearing the CallbackList during callback notification.
//
// This is possible to support, but not currently necessary.

namespace base {
namespace internal {
template <typename CallbackListImpl>
class CallbackListBase;
}  // namespace internal

template <typename Signature>
class OnceCallbackList;

template <typename Signature>
class RepeatingCallbackList;

// A trimmed-down version of ScopedClosureRunner that can be used to guarantee a
// closure is run on destruction. This is designed to be used by
// CallbackListBase to run CancelCallback() when this subscription dies;
// consumers can avoid callbacks on dead objects by ensuring the subscription
// returned by CallbackListBase::Add() does not outlive the bound object in the
// callback. A typical way to do this is to bind a callback to a member function
// on `this` and store the returned subscription as a member variable.
class [[nodiscard]] BASE_EXPORT CallbackListSubscription {
 public:
  CallbackListSubscription();
  CallbackListSubscription(CallbackListSubscription&& subscription);
  CallbackListSubscription& operator=(CallbackListSubscription&& subscription);
  ~CallbackListSubscription();

  explicit operator bool() const { return !!closure_; }

 private:
  template <typename T>
  friend class internal::CallbackListBase;

  explicit CallbackListSubscription(base::OnceClosure closure);

  void Run();

  OnceClosure closure_;
};

namespace internal {

// A traits class to break circular type dependencies between CallbackListBase
// and its subclasses.
template <typename CallbackList>
struct CallbackListTraits;

// NOTE: It's important that Callbacks provide iterator stability when items are
// added to the end, so e.g. a std::vector<> is not suitable here.
template <typename Signature>
struct CallbackListTraits<OnceCallbackList<Signature>> {
  using CallbackType = OnceCallback<Signature>;
  using Callbacks = std::list<CallbackType>;
};
template <typename Signature>
struct CallbackListTraits<RepeatingCallbackList<Signature>> {
  using CallbackType = RepeatingCallback<Signature>;
  using Callbacks = std::list<CallbackType>;
};

template <typename CallbackListImpl>
class CallbackListBase {
 public:
  using CallbackType =
      typename CallbackListTraits<CallbackListImpl>::CallbackType;

  CallbackListBase() = default;
  CallbackListBase(const CallbackListBase&) = delete;
  CallbackListBase& operator=(const CallbackListBase&) = delete;

  ~CallbackListBase() {
    // Destroying the list during iteration is unsupported and will cause a UAF.
    CHECK(!iterating_);
  }

  // Remove all callbacks. Must not be called while iterating.
  void Clear() {
    CHECK(!iterating_);
    if (empty()) {
      return;
    }
    // Invalidate `Subscription` callbacks, because they reference iterators
    // that are about to be invalid.
    weak_ptr_factory_.InvalidateWeakPtrs();
    callbacks_.clear();
    if (removal_callback_) {
      removal_callback_.Run();  // May delete |this|!
    }
  }

  // Registers |cb| for future notifications. Returns a CallbackListSubscription
  // whose destruction will cancel |cb|.
  [[nodiscard]] CallbackListSubscription Add(CallbackType cb) {
    DCHECK(!cb.is_null());
    return CallbackListSubscription(base::BindOnce(
        &CallbackListBase::CancelCallback, weak_ptr_factory_.GetWeakPtr(),
        callbacks_.insert(callbacks_.end(), std::move(cb))));
  }

  // Registers |cb| for future notifications. Provides no way for the caller to
  // cancel, so this is only safe for cases where the callback is guaranteed to
  // live at least as long as this list (e.g. if it's bound on the same object
  // that owns the list).
  // TODO(pkasting): Attempt to use Add() instead and see if callers can relax
  // other lifetime/ordering mechanisms as a result.
  void AddUnsafe(CallbackType cb) {
    DCHECK(!cb.is_null());
    callbacks_.push_back(std::move(cb));
  }

  // Registers |removal_callback| to be run after elements are removed from the
  // list of registered callbacks.
  void set_removal_callback(const RepeatingClosure& removal_callback) {
    removal_callback_ = removal_callback;
  }

  // Returns whether the list of registered callbacks is empty (from an external
  // perspective -- meaning no remaining callbacks are live).
  bool empty() const {
    return std::ranges::all_of(
        callbacks_, [](const auto& callback) { return callback.is_null(); });
  }

  // Calls all registered callbacks that are not canceled beforehand. If any
  // callbacks are unregistered, notifies any registered removal callback at the
  // end.
  //
  // Arguments must be copyable, since they must be supplied to all callbacks.
  // Move-only types would be destructively modified by passing them to the
  // first callback and not reach subsequent callbacks as intended.
  //
  // Notify() may be called re-entrantly, in which case the nested call
  // completes before the outer one continues. Callbacks are only ever added at
  // the end and canceled callbacks are not pruned from the list until the
  // outermost iteration completes, so existing iterators should never be
  // invalidated. However, this does mean that a callback added during a nested
  // call can be notified by outer calls -- meaning it will be notified about
  // things that happened before it was added -- if its subscription outlives
  // the reentrant Notify() call.
  template <typename... RunArgs>
  void Notify(RunArgs&&... args) {
    if (empty()) {
      return;  // Nothing to do.
    }

    {
      AutoReset<bool> iterating(&iterating_, true);

      // Skip any callbacks that are canceled during iteration.
      // NOTE: Since RunCallback() may call Add(), it's not safe to cache the
      // value of callbacks_.end() across loop iterations.
      const auto next_valid = [this](const auto it) {
        return std::find_if_not(it, callbacks_.end(), [](const auto& callback) {
          return callback.is_null();
        });
      };
      for (auto it = next_valid(callbacks_.begin()); it != callbacks_.end();
           it = next_valid(it)) {
        // NOTE: Intentionally does not call std::forward<RunArgs>(args)...,
        // since that would allow move-only arguments.
        static_cast<CallbackListImpl*>(this)->RunCallback(it++, args...);
      }
    }

    // Re-entrant invocations shouldn't prune anything from the list. This can
    // invalidate iterators from underneath higher call frames. It's safe to
    // simply do nothing, since the outermost frame will continue through here
    // and prune all null callbacks below.
    if (iterating_) {
      return;
    }

    // Any null callbacks remaining in the list were canceled due to
    // Subscription destruction during iteration, and can safely be erased now.
    const bool any_callbacks_erased = static_cast<CallbackListImpl*>(this)
                                          ->CleanUpNullCallbacksPostIteration();

    // Run |removal_callback_| if any callbacks were canceled or executed. Note
    // that simply comparing list sizes before and after iterating cannot be
    // done, since notification may result in Add()ing new callbacks as well as
    // canceling them.
    if (removal_callback_ && any_callbacks_erased) {
      removal_callback_.Run();  // May delete |this|!
    }
  }

 protected:
  using Callbacks = typename CallbackListTraits<CallbackListImpl>::Callbacks;

  // Holds non-null callbacks, which will be called during Notify().
  Callbacks callbacks_;

 private:
  // Cancels the callback pointed to by |it|, which is guaranteed to be valid.
  void CancelCallback(const typename Callbacks::iterator& it) {
    if (static_cast<CallbackListImpl*>(this)->CancelCallback(it, iterating_) &&
        removal_callback_) {
      removal_callback_.Run();  // May delete |this|!
    }
  }

  // Set while Notify() is traversing |callbacks_|.  Used primarily to avoid
  // invalidating iterators that may be in use.
  bool iterating_ = false;

  // Called after elements are removed from |callbacks_|.
  RepeatingClosure removal_callback_;

  WeakPtrFactory<CallbackListBase> weak_ptr_factory_{this};
};

}  // namespace internal

template <typename Signature>
class OnceCallbackList
    : public internal::CallbackListBase<OnceCallbackList<Signature>> {
 private:
  friend internal::CallbackListBase<OnceCallbackList>;
  using Traits = internal::CallbackListTraits<OnceCallbackList>;

  // Runs the current callback, which may cancel it or any other callbacks.
  template <typename... RunArgs>
  void RunCallback(typename Traits::Callbacks::iterator it, RunArgs&&... args) {
    // Do not splice here. Splicing during iteration breaks re-entrant Notify()
    // by invalidating the outer loop's iterator. Splicing is deferred to
    // CleanUpNullCallbacksPostIteration(), which is called when the outermost
    // Notify() finishes.

    // NOTE: Intentionally does not call std::forward<RunArgs>(args)...; see
    // comments in Notify().
    std::move(*it).Run(args...);
  }

  // Called during subscription destruction to cancel the callback. Returns true
  // if the callback was removed from the active list and the generic removal
  // callback should be executed. Returns false if the callback was already
  // executed, or if the erasure is deferred due to active iteration.
  bool CancelCallback(const typename Traits::Callbacks::iterator& it,
                      bool is_iterating) {
    if (is_iterating) {
      // During iteration, nodes cannot be safely erased from |callbacks_|
      // without invalidating iterators. They also cannot be spliced into
      // |null_callbacks_| right now. Thus, the node is reset and tracked for
      // erasure in CleanUpNullCallbacksPostIteration().
      it->Reset();
      pending_erasures_.push_back(it);
      return false;
    }

    if (it->is_null()) {
      // The callback already ran, so it's safely sitting in |null_callbacks_|.
      null_callbacks_.erase(it);
      return false;
    }

    // The callback hasn't run yet, so it's still in |callbacks_|.
    this->callbacks_.erase(it);
    return true;
  }

  // Performs post-iteration cleanup. Successfully executed callbacks (which
  // become null) are spliced into |null_callbacks_| to keep their
  // Subscriptions' iterators valid. Callbacks explicitly canceled during
  // iteration (tracked in |pending_erasures_|) are erased. Returns true if any
  // callbacks were erased or spliced out.
  bool CleanUpNullCallbacksPostIteration() {
    bool any_spliced = false;
    for (auto it = this->callbacks_.begin(); it != this->callbacks_.end();) {
      if (it->is_null()) {
        any_spliced = true;
        auto next = std::next(it);
        null_callbacks_.splice(null_callbacks_.end(), this->callbacks_, it);
        it = next;
      } else {
        ++it;
      }
    }

    bool any_erased = !pending_erasures_.empty();
    for (auto pending_it : pending_erasures_) {
      // Note: `pending_it` was originally an iterator into `callbacks_`, but
      // the node it points to has just been spliced into `null_callbacks_`. The
      // iterator itself remains valid and can now be used for erasure from
      // `null_callbacks_`.
      null_callbacks_.erase(pending_it);
    }
    pending_erasures_.clear();
    return any_spliced || any_erased;
  }

  // Holds null callbacks whose Subscriptions are still alive, so the
  // Subscriptions will still contain valid iterators.  Only needed for
  // OnceCallbacks, since RepeatingCallbacks are not canceled except by
  // Subscription destruction.
  typename Traits::Callbacks null_callbacks_;

  // Holds iterators for callbacks canceled during iteration.
  // Erasure is deferred to CleanUpNullCallbacksPostIteration() when iteration
  // completes to prevent invalidating iterators that an outer loop might hold.
  std::vector<typename Traits::Callbacks::iterator> pending_erasures_;
};

template <typename Signature>
class RepeatingCallbackList
    : public internal::CallbackListBase<RepeatingCallbackList<Signature>> {
 private:
  friend internal::CallbackListBase<RepeatingCallbackList>;
  using Traits = internal::CallbackListTraits<RepeatingCallbackList>;
  // Runs the current callback, which may cancel it or any other callbacks.
  template <typename... RunArgs>
  void RunCallback(typename Traits::Callbacks::iterator it, RunArgs&&... args) {
    // NOTE: Intentionally does not call std::forward<RunArgs>(args)...; see
    // comments in Notify().
    it->Run(args...);
  }

  // Called during subscription destruction to cancel the callback. Returns true
  // if the callback was removed from the active list and the generic removal
  // callback should be executed. Returns false if the callback was already
  // executed, or if the erasure is deferred due to active iteration.
  bool CancelCallback(const typename Traits::Callbacks::iterator& it,
                      bool is_iterating) {
    if (is_iterating) {
      // During iteration, nodes cannot be safely erased from |callbacks_|
      // without invalidating iterators. The node is reset and will be swept up
      // by CleanUpNullCallbacksPostIteration().
      it->Reset();
      return false;
    }

    this->callbacks_.erase(it);
    return true;
  }

  // Performs post-iteration cleanup by erasing all canceled callbacks. Returns
  // true if any callbacks were erased.
  bool CleanUpNullCallbacksPostIteration() {
    return std::erase_if(this->callbacks_,
                         [](const auto& cb) { return cb.is_null(); }) > 0;
  }
};

// Syntactic sugar to parallel that used for {Once,Repeating}Callbacks.
using OnceClosureList = OnceCallbackList<void()>;
using RepeatingClosureList = RepeatingCallbackList<void()>;

}  // namespace base

#endif  // BASE_CALLBACK_LIST_H_
