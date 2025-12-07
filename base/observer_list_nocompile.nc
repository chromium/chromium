// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/observer_list.h"

#include <memory>
#include <string>
#include <type_traits>

namespace base {

void CannotUseCheckedObserverInUncheckedList() {
  struct Observer : public CheckedObserver {
    void OnObserve() {}
  };
  ObserverList<Observer>::Unchecked list;
  for (auto& observer : list)  // expected-error@base/observer_list_internal.h:* {{CheckedObserver classes must not use ObserverList<T>::Unchecked}}
    observer.OnObserve();
}

void CannotUseUncheckedObserverInCheckedList() {
  struct UncheckedObserver {
    void OnObserve() {}
  };
  ObserverList<UncheckedObserver> list;
  for (auto& observer : list)  // expected-error@base/observer_list_internal.h:* {{Observers should inherit from base::CheckedObserver.}}
    observer.OnObserve();      // expected-error@*:* {{static_cast from 'base::CheckedObserver *' to 'UncheckedObserver *', which are not related by inheritance, is not allowed}}
}

// This test ensures that passing move-only types to ObserverList::Notify()
// fails compilation.
void NotifyWithMoveOnlyArgsFails() {
  // std::unique_ptr is move-only.
  auto str = std::make_unique<std::string>("123");

  struct TestObserverWithArgs : public CheckedObserver {
    void Observe(int, std::unique_ptr<std::string>) {}
  };

  ObserverList<TestObserverWithArgs> list;
  // expected-error@* {{no matching member function for call to 'Notify'}}
  list.Notify(&TestObserverWithArgs::Observe, 10, str);

  // expected-error@* {{no matching member function for call to 'Notify'}}
  list.Notify(&TestObserverWithArgs::Observe, 10, std::move(str));
}

}  // namespace base
