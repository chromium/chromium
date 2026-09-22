// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_OBSERVABLE_VALUE_H_
#define CHROME_BROWSER_GLIC_COMMON_OBSERVABLE_VALUE_H_

#include <type_traits>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"

namespace glic {

// A read-only view of an ObservableValue that allows reading the current value
// and subscribing to changes, but does not allow modifying the value.
template <typename T>
class ObservableValueView {
 public:
  using ValueType = T;
  using Callback = base::RepeatingCallback<void(const T&)>;
  using CallbackList = base::RepeatingCallbackList<void(const T&)>;

  ObservableValueView(const ObservableValueView&) = delete;
  ObservableValueView& operator=(const ObservableValueView&) = delete;
  virtual ~ObservableValueView() = default;

  // Access the current value.
  const T& get() const { return value_; }
  const T& value() const { return value_; }
  const T& operator*() const { return value_; }
  const T* operator->() const { return &value_; }

  // Registers a callback to be called whenever the value changes.
  base::CallbackListSubscription AddObserver(Callback callback) {
    return callback_list_.Add(std::move(callback));
  }

  // Registers a callback and immediately invokes it with the current value.
  base::CallbackListSubscription AddObserverAndNotify(Callback callback) {
    callback.Run(value_);
    return callback_list_.Add(std::move(callback));
  }

  // Registers a parameterless closure to be called whenever the value changes.
  base::CallbackListSubscription AddObserver(base::RepeatingClosure closure) {
    return callback_list_.Add(base::IgnoreArgs<const T&>(std::move(closure)));
  }

  // Registers a parameterless closure and immediately invokes it.
  base::CallbackListSubscription AddObserverAndNotify(
      base::RepeatingClosure closure) {
    closure.Run();
    return AddObserver(std::move(closure));
  }

  bool HasObservers() const { return !callback_list_.empty(); }

 protected:
  ObservableValueView()
    requires std::is_default_constructible_v<T>
      : value_() {}

  explicit ObservableValueView(T initial_value)
      : value_(std::move(initial_value)) {}

  T value_;
  CallbackList callback_list_;
};

// Wraps a value of type `T` and notifies registered observers or callbacks
// whenever the value is modified.
template <typename T>
class ObservableValue : public ObservableValueView<T> {
 public:
  using Base = ObservableValueView<T>;

  ObservableValue()
    requires std::is_default_constructible_v<T>
      : Base() {}

  explicit ObservableValue(T initial_value) : Base(std::move(initial_value)) {}

  ~ObservableValue() override = default;

  // Sets a new value. If `T` supports `operator==`, observers are only notified
  // if the new value is different from the current value.
  void Set(T new_value) {
    if constexpr (requires(const T& a, const T& b) { a == b; }) {
      if (this->value_ == new_value) {
        return;
      }
    }
    this->value_ = std::move(new_value);
    Notify();
  }

  // Manually notifies observers with the current value.
  void Notify() { this->callback_list_.Notify(this->value_); }
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_OBSERVABLE_VALUE_H_
