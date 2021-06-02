// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBSERVER_LIST_THREADSAFE_H_
#define BASE_OBSERVER_LIST_THREADSAFE_H_

#include <unordered_map>
#include <utility>

#include "base/base_export.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"

///////////////////////////////////////////////////////////////////////////////
//
// OVERVIEW:
//
//   A thread-safe container for a list of observers. This is similar to the
//   observer_list (see observer_list.h), but it is more robust for multi-
//   threaded situations.
//
//   The following use cases are supported:
//    * Observers can register for notifications from any sequence. They are
//      always notified on the sequence from which they were registered.
//    * Any sequence may trigger a notification via Notify().
//    * Observers can remove themselves from the observer list inside of a
//      callback.
//    * If one sequence is notifying observers concurrently with an observer
//      removing itself from the observer list, the notifications will be
//      silently dropped.
//
//   The drawback of the threadsafe observer list is that notifications are not
//   as real-time as the non-threadsafe version of this class. Notifications
//   will always be done via PostTask() to another sequence, whereas with the
//   non-thread-safe ObserverList, notifications happen synchronously.
//
//   Note: this class previously supported synchronous notifications for
//   same-sequence observers, but it was error-prone and removed in
//   crbug.com/1193750, think twice before re-considering this paradigm.
//
///////////////////////////////////////////////////////////////////////////////

namespace base {
namespace internal {

class BASE_EXPORT ObserverListThreadSafeBase
    : public RefCountedThreadSafe<ObserverListThreadSafeBase> {
 public:
  ObserverListThreadSafeBase() = default;
  ObserverListThreadSafeBase(const ObserverListThreadSafeBase&) = delete;
  ObserverListThreadSafeBase& operator=(const ObserverListThreadSafeBase&) =
      delete;

 protected:
  template <typename ObserverType, typename Method>
  struct Dispatcher;

  template <typename ObserverType, typename ReceiverType, typename... Params>
  struct Dispatcher<ObserverType, void (ReceiverType::*)(Params...)> {
    static void Run(void (ReceiverType::*m)(Params...),
                    Params... params,
                    ObserverType* obj) {
      (obj->*m)(std::forward<Params>(params)...);
    }
  };

  struct NotificationDataBase {
    NotificationDataBase(void* observer_list_in, const Location& from_here_in)
        : observer_list(observer_list_in), from_here(from_here_in) {}

    void* observer_list;
    Location from_here;
  };

  virtual ~ObserverListThreadSafeBase() = default;

  static LazyInstance<ThreadLocalPointer<const NotificationDataBase>>::Leaky
      tls_current_notification_;

 private:
  friend class RefCountedThreadSafe<ObserverListThreadSafeBase>;
};

}  // namespace internal

template <class ObserverType>
class ObserverListThreadSafe : public internal::ObserverListThreadSafeBase {
 public:
  enum class AddObserverResult {
    kBecameNonEmpty,
    kWasAlreadyNonEmpty,
  };
  enum class RemoveObserverResult {
    kWasOrBecameEmpty,
    kRemainsNonEmpty,
  };

  ObserverListThreadSafe() = default;
  explicit ObserverListThreadSafe(ObserverListPolicy policy)
      : policy_(policy) {}
  ObserverListThreadSafe(const ObserverListThreadSafe&) = delete;
  ObserverListThreadSafe& operator=(const ObserverListThreadSafe&) = delete;

  // Adds |observer| to the list. |observer| must not already be in the list.
  AddObserverResult AddObserver(ObserverType* observer) {
    DCHECK(SequencedTaskRunnerHandle::IsSet())
        << "An observer can only be registered when SequencedTaskRunnerHandle "
           "is set. If this is in a unit test, you're likely merely missing a "
           "base::test::(SingleThread)TaskEnvironment in your fixture. "
           "Otherwise, try running this code on a named thread (main/UI/IO) or "
           "from a task posted to a base::SequencedTaskRunner or "
           "base::SingleThreadTaskRunner.";

    AutoLock auto_lock(lock_);

    bool was_empty = observers_.empty();

    // Add |observer| to the list of observers.
    DCHECK(!Contains(observers_, observer));
    const scoped_refptr<SequencedTaskRunner> task_runner =
        SequencedTaskRunnerHandle::Get();
    // Each observer gets a unique identifier. These unique identifiers are used
    // to avoid execution of pending posted-tasks over removed or released
    // observers.
    const size_t observer_id = ++observer_id_counter_;
    ObserverTaskRunnerInfo task_info = {task_runner, observer_id};
    observers_[observer] = std::move(task_info);

    // If this is called while a notification is being dispatched on this thread
    // and |policy_| is ALL, |observer| must be notified (if a notification is
    // being dispatched on another thread in parallel, the notification may or
    // may not make it to |observer| depending on the outcome of the race to
    // |lock_|).
    if (policy_ == ObserverListPolicy::ALL) {
      const NotificationDataBase* current_notification =
          tls_current_notification_.Get().Get();
      if (current_notification && current_notification->observer_list == this) {
        const NotificationData* notification_data =
            static_cast<const NotificationData*>(current_notification);
        task_runner->PostTask(
            current_notification->from_here,
            BindOnce(&ObserverListThreadSafe<ObserverType>::NotifyWrapper, this,
                     observer,
                     NotificationData(this, observer_id,
                                      current_notification->from_here,
                                      notification_data->method)));
      }
    }

    return was_empty ? AddObserverResult::kBecameNonEmpty
                     : AddObserverResult::kWasAlreadyNonEmpty;
  }

  // Remove an observer from the list if it is in the list.
  //
  // If a notification was sent to the observer but hasn't started to run yet,
  // it will be aborted. If a notification has started to run, removing the
  // observer won't stop it.
  RemoveObserverResult RemoveObserver(ObserverType* observer) {
    AutoLock auto_lock(lock_);
    observers_.erase(observer);
    return observers_.empty() ? RemoveObserverResult::kWasOrBecameEmpty
                              : RemoveObserverResult::kRemainsNonEmpty;
  }

  // Verifies that the list is currently empty (i.e. there are no observers).
  void AssertEmpty() const {
#if DCHECK_IS_ON()
    AutoLock auto_lock(lock_);
    DCHECK(observers_.empty());
#endif
  }

  // Asynchronously invokes a callback on all observers, on their registration
  // sequence. You cannot assume that at the completion of the Notify call that
  // all Observers have been Notified. The notification may still be pending
  // delivery.
  template <typename Method, typename... Params>
  void Notify(const Location& from_here, Method m, Params&&... params) {
    RepeatingCallback<void(ObserverType*)> method =
        BindRepeating(&Dispatcher<ObserverType, Method>::Run, m,
                      std::forward<Params>(params)...);

    AutoLock lock(lock_);
    for (const auto& observer : observers_) {
      observer.second.task_runner->PostTask(
          from_here,
          BindOnce(&ObserverListThreadSafe<ObserverType>::NotifyWrapper, this,
                   observer.first,
                   NotificationData(this, observer.second.observer_id,
                                    from_here, method)));
    }
  }

 private:
  friend class RefCountedThreadSafe<ObserverListThreadSafeBase>;

  struct NotificationData : public NotificationDataBase {
    NotificationData(ObserverListThreadSafe* observer_list_in,
                     size_t observer_id_in,
                     const Location& from_here_in,
                     const RepeatingCallback<void(ObserverType*)>& method_in)
        : NotificationDataBase(observer_list_in, from_here_in),
          method(method_in),
          observer_id(observer_id_in) {}

    RepeatingCallback<void(ObserverType*)> method;
    size_t observer_id;
  };

  ~ObserverListThreadSafe() override = default;

  void NotifyWrapper(ObserverType* observer,
                     const NotificationData& notification) {
    {
      AutoLock auto_lock(lock_);

      // Check whether the observer still needs a notification.
      DCHECK_EQ(notification.observer_list, this);
      auto it = observers_.find(observer);
      if (it == observers_.end() ||
          it->second.observer_id != notification.observer_id) {
        return;
      }
      DCHECK(it->second.task_runner->RunsTasksInCurrentSequence());
    }

    // Keep track of the notification being dispatched on the current thread.
    // This will be used if the callback below calls AddObserver().
    //
    // Note: |tls_current_notification_| may not be nullptr if this runs in a
    // nested loop started by a notification callback. In that case, it is
    // important to save the previous value to restore it later.
    auto& tls_current_notification = tls_current_notification_.Get();
    const NotificationDataBase* const previous_notification =
        tls_current_notification.Get();
    tls_current_notification.Set(&notification);

    // Invoke the callback.
    notification.method.Run(observer);

    // Reset the notification being dispatched on the current thread to its
    // previous value.
    tls_current_notification.Set(previous_notification);
  }

  const ObserverListPolicy policy_ = ObserverListPolicy::ALL;

  mutable Lock lock_;

  size_t observer_id_counter_ GUARDED_BY(lock_) = 0;

  struct ObserverTaskRunnerInfo {
    scoped_refptr<SequencedTaskRunner> task_runner;
    size_t observer_id = 0;
  };

  // Keys are observers. Values are the SequencedTaskRunners on which they must
  // be notified.
  std::unordered_map<ObserverType*, ObserverTaskRunnerInfo> observers_
      GUARDED_BY(lock_);
};

}  // namespace base

#endif  // BASE_OBSERVER_LIST_THREADSAFE_H_
