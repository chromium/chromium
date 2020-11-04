// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_OBSERVATION_H_
#define BASE_SCOPED_OBSERVATION_H_

#include <stddef.h>

#include "base/check_op.h"

namespace base {

// ScopedObservation is used to keep track of a single observation.
// When ScopedObservation is destroyed, it removes the registered observation,
// if any. Basic example (as a member variable):
//
//   class MyFooObserver : public FooObserver {
//     ...
//    private:
//     ScopedObservation<Foo, FooObserver> foo_observation_{this};
//   };
//
// For cases with methods not named AddObserver/RemoveObserver:
//
//   class MyFooStateObserver : public FooStateObserver {
//     ...
//    private:
//     ScopedObservation<Foo,
//                    FooStateObserver,
//                    &Foo::AddStateObserver,
//                    &Foo::RemoveStateObserver>
//       observed_foo_{this};
//   };
//
// See also base::ScopedObserver to manage observations from multiple sources.
template <class Source,
          class Observer,
          void (Source::*AddObsFn)(Observer*) = &Source::AddObserver,
          void (Source::*RemoveObsFn)(Observer*) = &Source::RemoveObserver>
class ScopedObservation {
 public:
  explicit ScopedObservation(Observer* observer) : observer_(observer) {}
  ScopedObservation(const ScopedObservation&) = delete;
  ScopedObservation& operator=(const ScopedObservation&) = delete;
  ~ScopedObservation() {
    if (IsObserving())
      RemoveObservation();
  }

  // Adds the object passed to the constructor as an observer on |source|.
  // IsObserving() must be false.
  void Observe(Source* source) {
    DCHECK_EQ(source_, nullptr);
    source_ = source;
    (source_->*AddObsFn)(observer_);
  }

  // Remove the object passed to the constructor as an observer from |source|.
  void RemoveObservation() {
    DCHECK_NE(source_, nullptr);
    (source_->*RemoveObsFn)(observer_);
    source_ = nullptr;
  }

  // Returns true if any source is being observed.
  bool IsObserving() const { return source_ != nullptr; }

  // Returns true if |source| is being observed.
  bool IsObservingSource(Source* source) const { return source_ == source; }

 private:
  Observer* const observer_;

  // The observed source, if any.
  Source* source_ = nullptr;
};

}  // namespace base

#endif  // BASE_SCOPED_OBSERVATION_H_
