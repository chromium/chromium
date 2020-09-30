// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_SHARED_VECTOR_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_SHARED_VECTOR_H_

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"

namespace reporting {

// SharedVector wraps a |std::vector| and ensures access happens on a
// SequencedTaskRunner.
template <typename VectorType>
class SharedVector
    : public base::RefCountedThreadSafe<SharedVector<VectorType>> {
 public:
  static scoped_refptr<SharedVector<VectorType>> Create() {
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner{
        base::ThreadPool::CreateSequencedTaskRunner({})};
    return base::WrapRefCounted(
        new SharedVector<VectorType>(sequenced_task_runner));
  }

  void PushBack(VectorType item,
                base::OnceCallback<void()> push_back_complete_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedVector::OnPushBack, this, std::move(item),
                       std::move(push_back_complete_cb)));
  }

  // Erase will call erase on all elements that return true for the
  // |predicate_cb|.
  void Erase(base::RepeatingCallback<bool(const VectorType&)> predicate_cb,
             base::OnceCallback<void(size_t)> remove_complete_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedVector::OnErase, this, std::move(predicate_cb),
                       std::move(remove_complete_cb)));
  }

  // Provided as the nearest equivalent to std::vector::find.  A regular find
  // operation may be invalid by the time a caller is notified of its existence.
  // |predicate_cb| is called on each element. If |predicate_cb| returns true
  // |found_cb| is called on the same element, ending the search.
  // |not_found_cb| is called if no elements return true.
  void ExecuteIfFound(
      base::RepeatingCallback<bool(const VectorType&)> predicate_cb,
      base::OnceCallback<void(VectorType&)> found_cb,
      base::OnceCallback<void()> not_found_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SharedVector::OnExecuteIfFound, this,
                                  std::move(predicate_cb), std::move(found_cb),
                                  std::move(not_found_cb)));
  }

  // Iterates over each element in |vector_|, and calls |predicate_cb|. If
  // |predicate_cb| returns true, |element_executor| will be called on the same
  // element and iteration will continue. At the end of iteration
  // |execute_complete_cb| will be called.
  // A default |predicate_cb| is provided that always returns true.
  void ExecuteOnEachElement(
      base::RepeatingCallback<void(VectorType&)> element_executor,
      base::OnceCallback<void()> execute_complete_cb,
      base::RepeatingCallback<bool(const VectorType&)> predicate_cb =
          base::BindRepeating([](const VectorType&) { return true; })) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SharedVector::OnExecuteOnEachElement, this,
                                  std::move(element_executor),
                                  std::move(execute_complete_cb),
                                  std::move(predicate_cb)));
  }

  void IsEmpty(base::OnceCallback<void(bool)> get_empty_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SharedVector::OnIsEmpty, this,
                                  std::move(get_empty_cb)));
  }

 protected:
  virtual ~SharedVector() = default;

 private:
  friend class base::RefCountedThreadSafe<SharedVector<VectorType>>;

  explicit SharedVector(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : sequenced_task_runner_(sequenced_task_runner) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  void OnPushBack(VectorType item,
                  base::OnceCallback<void()> push_back_complete_cb) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    vector_.push_back(std::move(item));
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](base::OnceCallback<void()> push_back_complete_cb) {
              std::move(push_back_complete_cb).Run();
            },
            std::move(push_back_complete_cb)));
  }

  void OnErase(base::RepeatingCallback<bool(const VectorType&)> predicate_cb,
               base::OnceCallback<void(size_t)> remove_complete_cb) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    size_t number_erased = 0;
    for (auto it = vector_.begin(); it != vector_.end();) {
      if (predicate_cb.Run(*it)) {
        it = vector_.erase(it);
        number_erased++;
      } else {
        it++;
      }
    }
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](base::OnceCallback<void(size_t)> remove_complete_cb,
               size_t number_erased) {
              std::move(remove_complete_cb).Run(number_erased);
            },
            std::move(remove_complete_cb), number_erased));
  }

  void OnExecuteIfFound(
      base::RepeatingCallback<bool(const VectorType&)> predicate_cb,
      base::OnceCallback<void(VectorType&)> found_cb,
      base::OnceCallback<void()> not_found_cb) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (VectorType& element : vector_) {
      if (predicate_cb.Run(element)) {
        std::move(found_cb).Run(element);
        return;
      }
    }
    base::ThreadPool::PostTask(FROM_HERE, {base::TaskPriority::BEST_EFFORT},
                               base::BindOnce(
                                   [](base::OnceCallback<void()> not_found_cb) {
                                     std::move(not_found_cb).Run();
                                   },
                                   std::move(not_found_cb)));
  }

  void OnExecuteOnEachElement(
      base::RepeatingCallback<void(VectorType&)> element_executor,
      base::OnceCallback<void()> execute_complete_cb,
      base::RepeatingCallback<bool(const VectorType&)> predicate_cb) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (VectorType& element : vector_) {
      if (predicate_cb.Run(element)) {
        element_executor.Run(element);
      } else {
        break;
      }
    }
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](base::OnceCallback<void()> execute_complete_cb) {
              std::move(execute_complete_cb).Run();
            },
            std::move(execute_complete_cb)));
  }

  void OnIsEmpty(base::OnceCallback<void(bool)> is_empty_cb) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](base::OnceCallback<void(bool)> is_empty_cb, bool is_empty) {
              std::move(is_empty_cb).Run(is_empty);
            },
            std::move(is_empty_cb), vector_.empty()));
  }

  std::vector<VectorType> vector_;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_SHARED_VECTOR_H_
