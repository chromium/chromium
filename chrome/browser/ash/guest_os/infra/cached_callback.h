// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_INFRA_CACHED_CALLBACK_H_
#define CHROME_BROWSER_ASH_GUEST_OS_INFRA_CACHED_CALLBACK_H_

#include <memory>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"

namespace guest_os {

// Manages several racing callbacks which all attempt to access the same
// (logical) object.
//
// This class is used when you have multiple potential callers for an
// asynchronous operation, and you want them all to agree on the outcome of that
// operation. The first callback that comes in triggers the "real" async
// operation, and subsequent callbacks are queued. If the "real" operation
// succeeds, all currently queued and future callbacks are invoked with a handle
// to that success. If it fails, all currently queued callbacks are notified of
// the failure but future ones will retry the "real" async operation.
//
// Internally this class tries to create the "real" result T as a unique_ptr,
// but exposes it to its own clients as a T*. For the errors E it is advisable
// to use something movable and copyable, preferably an enum.
template <typename T, typename E>
class CachedCallback {
 public:
  // As a convenience this class provides a default implementation of the
  // Reject() method, which is used when the cache is deleted while callbacks
  // are in-flight. In this case we default-construct an E for them.
  //
  // We can avoid this requirement using std::enable_if shenanigans but
  // enforcing the below is cleaner.
  static_assert(
      std::is_default_constructible<E>::value,
      "Cached callbacks must have a default constructible error type");

  using Result = base::expected<T*, E>;

  using Callback = base::OnceCallback<void(Result result)>;

  virtual ~CachedCallback() {
    if (!queued_callbacks_.empty()) {
      Finish(base::unexpected(Reject()));
    }
  }

  // Request access to the cached result.
  void Get(Callback callback) {
    if (real_object_) {
      std::move(callback).Run(Result(real_object_.get()));
      return;
    }
    queued_callbacks_.push_back(std::move(callback));
    // If this is the first callback, spawn the real one. This must happen after
    // enqueueing the callback in case the factory returns synchronously.
    if (queued_callbacks_.size() == 1) {
      Build(base::BindOnce(&CachedCallback<T, E>::OnRealResultFound,
                           weak_factory_.GetWeakPtr()));
    }
  }

  // Returns the cached result if it exists, nullptr otherwise.
  T* MaybeGet() const {
    if (real_object_) {
      return real_object_.get();
    }
    return nullptr;
  }

  // Clears the stored real result (if one exists) and returns it (or null).
  std::unique_ptr<T> Invalidate() {
    std::unique_ptr<T> real = std::move(real_object_);
    real_object_.reset();
    return std::move(real);
  }

  void CacheForTesting(std::unique_ptr<T> real) {
    OnRealResultFound(RealResult(std::move(real)));
  }

 protected:
  using RealResult = base::expected<std::unique_ptr<T>, E>;

  using RealCallback = base::OnceCallback<void(RealResult)>;

  // Used to construct the "real" result, which will be owned by this class.
  virtual void Build(RealCallback callback) = 0;

  // In cases where the cache determines that a request can not be fulfilled,
  // this error is returned to callers automatically. For example, it is used if
  // the cache is destroyed while requests are in flight.
  //
  // Unless overridden, a default-constructed E is used.
  virtual E Reject() { return E{}; }

  // Helper template to construct successful RealResults more conveniently. It
  // is static so that lambdas defined by the subclass can use them.
  template <typename... TT>
  static RealResult Success(TT&&... tt) {
    return base::ok(std::make_unique<T>(std::forward<TT>(tt)...));
  }

  // Similar to Success, but for error results.
  template <typename... EE>
  static RealResult Failure(EE&&... ee) {
    return base::unexpected(std::forward<EE>(ee)...);
  }

 private:
  void OnRealResultFound(RealResult real_result) {
    if (!real_result.has_value()) {
      Finish(base::unexpected(real_result.error()));
      return;
    }
    real_object_ = std::move(real_result.value());
    Finish(base::ok(real_object_.get()));
  }

  void Finish(Result res) {
    while (!queued_callbacks_.empty()) {
      std::move(queued_callbacks_.back()).Run(res);
      queued_callbacks_.pop_back();
    }
  }

  std::unique_ptr<T> real_object_;
  std::vector<Callback> queued_callbacks_;
  base::WeakPtrFactory<CachedCallback<T, E>> weak_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_INFRA_CACHED_CALLBACK_H_
