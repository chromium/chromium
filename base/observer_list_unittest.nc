// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <type_traits>

#include "base/observer_list.h"

namespace base {

#if defined(NCTEST_CHECKED_OBSERVER_USING_UNCHECKED_LIST)  // [r"fatal error: static assertion failed due to requirement '!std::is_base_of<base::CheckedObserver, Observer>::value': CheckedObserver classes must not use ObserverList<T>::Unchecked."]

void WontCompile() {
  struct Observer : public CheckedObserver {
    void OnObserve() {}
  };
  ObserverList<Observer>::Unchecked list;
  for (auto& observer: list)
    observer.OnObserve();
}

#elif defined(NCTEST_UNCHECKED_OBSERVER_USING_CHECKED_LIST)  // [r"fatal error: static assertion failed due to requirement 'std::is_base_of<base::CheckedObserver, UncheckedObserver>::value': Observers should inherit from base::CheckedObserver. Use ObserverList<T>::Unchecked to observe with raw pointers."]

void WontCompile() {
  struct UncheckedObserver {
    void OnObserve() {}
  };
  ObserverList<UncheckedObserver> list;
  for (auto& observer: list)
    observer.OnObserve();
}

#endif

}  // namespace base
