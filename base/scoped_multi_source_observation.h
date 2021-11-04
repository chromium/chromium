// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SCOPED_MULTI_SOURCE_OBSERVATION_H_
#define BASE_SCOPED_MULTI_SOURCE_OBSERVATION_H_

#include <stddef.h>

#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"

namespace base {

// ScopedMultiSourceObservation is used to keep track of plural observation,
// e.g. where an observer observes more than a single source.
//
// Use base::ScopedObservation for objects that observe only a single source.
//
// When ScopedMultiSourceObservation is destroyed, it removes the object as an
// observer from all sources it has been added to.
// Basic example (as a member variable):
//
//   class MyFooObserver : public FooObserver {
//     ...
//    private:
//     ScopedMultiSourceObservation<Foo, FooObserver> foo_observations_{this};
//   };
//
//   MyFooObserver::OnFooCreated(Foo* foo) {
//     foo_observations_.AddObservation(foo);
//   }
//
// For cases with methods not named AddObserver/RemoveObserver:
//
//   class MyFooStateObserver : public FooStateObserver {
//     ...
//    private:
//      ScopedMultiSourceObservation<Foo,
//                                  FooStateObserver,
//                                  &Foo::AddStateObserver,
//                                  &Foo::RemoveStateObserver>
//          foo_observations_{this};
//   };
template <class Source,
          class Observer,
          void (Source::*AddObsFn)(Observer*) = &Source::AddObserver,
          void (Source::*RemoveObsFn)(Observer*) = &Source::RemoveObserver>
class ScopedMultiSourceObservation {
 public:
  explicit ScopedMultiSourceObservation(Observer* observer)
      : observer_(observer) {}
  ScopedMultiSourceObservation(const ScopedMultiSourceObservation&) = delete;
  ScopedMultiSourceObservation& operator=(const ScopedMultiSourceObservation&) =
      delete;
  ~ScopedMultiSourceObservation() { RemoveAllObservations(); }

  // Adds the object passed to the constructor as an observer on |source|.
  void AddObservation(Source* source) {
    sources_.push_back(source);
    (source->*AddObsFn)(observer_);
  }

  // Remove the object passed to the constructor as an observer from |source|.
  void RemoveObservation(Source* source) {
    auto it = base::ranges::find(sources_, source);
    DCHECK(it != sources_.end());
    sources_.erase(it);
    (source->*RemoveObsFn)(observer_);
  }

  // Remove the object passed to the constructor as an observer from all sources
  // it's observing.
  void RemoveAllObservations() {
    for (Source* source : sources_)
      (source->*RemoveObsFn)(observer_);
    sources_.clear();
  }

  // Returns true if any source is being observed.
  bool IsObservingAnySource() const { return !sources_.empty(); }

  // Returns true if |source| is being observed.
  bool IsObservingSource(Source* source) const {
    DCHECK(source);
    return base::Contains(sources_, source);
  }

  // Returns the number of sources being observed.
  size_t GetSourcesCount() const { return sources_.size(); }

 private:
  Observer* const observer_;

  std::vector<Source*> sources_;
};

}  // namespace base

#endif  // BASE_SCOPED_MULTI_SOURCE_OBSERVATION_H_
