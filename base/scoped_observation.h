// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_OBSERVATION_H_
#define BASE_SCOPED_OBSERVATION_H_

#include <stddef.h>

#include "base/check_op.h"

namespace base {

// ScopedObservation is used to keep track of singular observation, e.g.
// where an observer observes a single source only.
//
// Use base::ScopedMultiSourceObservation for objects that observe multiple
// sources.
//
// When ScopedObservation is destroyed, it removes the registered observation,
// if any. Basic example (as a member variable):
//
//   class MyFooObserver : public FooObserver {
//     ...
//    private:
//     ScopedObservation<Foo, FooObserver> foo_observation_{this};
//   };
//
//   MyFooObserver::MyFooObserver(Foo* foo) {
//     foo_observation_.Observe(foo);
//   }
//
// For cases with methods not named AddObserver/RemoveObserver:
//
//   class MyFooStateObserver : public FooStateObserver {
//     ...
//    private:
//     ScopedObservation<Foo,
//                       FooStateObserver,
//                       &Foo::AddStateObserver,
//                       &Foo::RemoveStateObserver>
//       foo_observation_{this};
//   };
template <class Source,
          class Observer,
          void (Source::*AddObsFn)(Observer*) = &Source::AddObserver,
          void (Source::*RemoveObsFn)(Observer*) = &Source::RemoveObserver>
class ScopedObservation {
 public:
  explicit ScopedObservation(Observer* observer) : observer_(observer) {}
  ScopedObservation(const ScopedObservation&) = delete;
  ScopedObservation& operator=(const ScopedObservation&) = delete;
  ~ScopedObservation() { Reset(); }

  // Adds the object passed to the constructor as an observer on |source|.
  // IsObserving() must be false.
  void Observe(Source* source) {
    // TODO(https://crbug.com/1145565): Make this a DCHECK once ScopedObserver
    //     has been fully retired.
    CHECK_EQ(source_, nullptr);
    source_ = source;
    (source_->*AddObsFn)(observer_);
  }

  // Remove the object passed to the constructor as an observer from |source_|
  // if currently observing. Does nothing otherwise.
  void Reset() {
    if (source_) {
      (source_->*RemoveObsFn)(observer_);
      source_ = nullptr;
    }
  }

  // Returns true if any source is being observed.
  bool IsObserving() const { return source_ != nullptr; }

  // Returns true if |source| is being observed.
  bool IsObservingSource(Source* source) const {
    DCHECK(source);
    return source_ == source;
  }

 private:
  Observer* const observer_;

  // The observed source, if any.
  Source* source_ = nullptr;
};

}  // namespace base

#endif  // BASE_SCOPED_OBSERVATION_H_
