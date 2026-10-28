// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_OBSERVABLE_VALUE_H_
#define CHROME_BROWSER_GLIC_COMMON_OBSERVABLE_VALUE_H_

#include <concepts>
#include <type_traits>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"

namespace glic {

template <typename T>
class ObservedValue;

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

  friend bool operator==(const ObservableValueView& lhs,
                         const ObservableValueView& rhs)
    requires requires(const T& a, const T& b) {
      { a == b } -> std::convertible_to<bool>;
    }
  {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator==(const ObservableValueView& lhs, const T& rhs)
    requires requires(const T& a, const T& b) {
      { a == b } -> std::convertible_to<bool>;
    }
  {
    return lhs.value_ == rhs;
  }

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

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserver(
      Receiver* receiver,
      void (Receiver::*method)(const T&)) {
    return AddObserver(base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserver(
      Receiver* receiver,
      void (Receiver::*method)()) {
    return AddObserver(base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserver(
      const Receiver* receiver,
      void (Receiver::*method)(const T&) const) {
    return AddObserver(base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserver(
      const Receiver* receiver,
      void (Receiver::*method)() const) {
    return AddObserver(base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserverAndNotify(
      Receiver* receiver,
      void (Receiver::*method)(const T&)) {
    return AddObserverAndNotify(
        base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserverAndNotify(
      Receiver* receiver,
      void (Receiver::*method)()) {
    return AddObserverAndNotify(
        base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserverAndNotify(
      const Receiver* receiver,
      void (Receiver::*method)(const T&) const) {
    return AddObserverAndNotify(
        base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver>
  [[nodiscard]] base::CallbackListSubscription AddObserverAndNotify(
      const Receiver* receiver,
      void (Receiver::*method)() const) {
    return AddObserverAndNotify(
        base::BindRepeating(method, base::Unretained(receiver)));
  }

  template <typename Receiver, typename Method>
  [[nodiscard]] ObservedValue<T> Observe(Receiver* receiver, Method method);

  // Note: The initial notification fires before the returned ObservedValue<T>
  // finishes construction, so callbacks that read the ObservedValue<T> member
  // on `receiver` (rather than taking `const T&`) should use `Observe()` and
  // invoke their update method after the member is initialized.
  template <typename Receiver, typename Method>
  [[nodiscard]] ObservedValue<T> ObserveAndNotify(Receiver* receiver,
                                                  Method method);

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

// Holds a non-null reference to an `ObservableValueView<T>` together with an
// active `base::CallbackListSubscription`.
template <typename T>
class ObservedValue {
 public:
  ObservedValue(ObservableValueView<T>& source,
                base::CallbackListSubscription subscription)
      : source_(source), subscription_(std::move(subscription)) {}

  template <typename Receiver, typename Method>
  ObservedValue(ObservableValueView<T>& source,
                Receiver* receiver,
                Method method)
      : source_(source), subscription_(source.AddObserver(receiver, method)) {}

  ObservedValue(ObservedValue&&) = default;
  ObservedValue& operator=(ObservedValue&&) = default;
  ObservedValue(const ObservedValue&) = delete;
  ObservedValue& operator=(const ObservedValue&) = delete;
  ~ObservedValue() = default;

  const T& get() const { return source_->get(); }
  const T& value() const { return source_->value(); }
  const T& operator*() const { return **source_; }
  const T* operator->() const { return source_->operator->(); }

  friend bool operator==(const ObservedValue& lhs, const ObservedValue& rhs)
    requires requires(const T& a, const T& b) {
      { a == b } -> std::convertible_to<bool>;
    }
  {
    return lhs.get() == rhs.get();
  }

  friend bool operator==(const ObservedValue& lhs,
                         const ObservableValueView<T>& rhs)
    requires requires(const T& a, const T& b) {
      { a == b } -> std::convertible_to<bool>;
    }
  {
    return lhs.get() == rhs.get();
  }

  friend bool operator==(const ObservedValue& lhs, const T& rhs)
    requires requires(const T& a, const T& b) {
      { a == b } -> std::convertible_to<bool>;
    }
  {
    return lhs.get() == rhs;
  }

 private:
  raw_ref<ObservableValueView<T>> source_;
  base::CallbackListSubscription subscription_;
};

template <typename T>
template <typename Receiver, typename Method>
ObservedValue<T> ObservableValueView<T>::Observe(Receiver* receiver,
                                                 Method method) {
  return ObservedValue<T>(*this, AddObserver(receiver, method));
}

template <typename T>
template <typename Receiver, typename Method>
ObservedValue<T> ObservableValueView<T>::ObserveAndNotify(Receiver* receiver,
                                                          Method method) {
  return ObservedValue<T>(*this, AddObserverAndNotify(receiver, method));
}

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

  // Returns a view to this value. For convenience, allows access while const,
  // as a view cannot change the value, but only observe it.
  ObservableValueView<T>& view() const {
    return const_cast<ObservableValueView<T>&>(
        static_cast<const ObservableValueView<T>&>(*this));
  }
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_OBSERVABLE_VALUE_H_
