// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/memory/weak_ptr.h"

namespace base {

struct Producer {
  WeakPtr<Producer> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  WeakPtrFactory<Producer> weak_ptr_factory_{this};
};
struct DerivedProducer : Producer {};
struct OtherDerivedProducer : Producer {};
struct Unrelated {};
struct DerivedUnrelated : Unrelated {};

void DowncastDisallowed() {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  {
    WeakPtr<DerivedProducer> derived_ptr = ptr;  // expected-error {{no viable conversion from 'WeakPtr<Producer>' to 'WeakPtr<DerivedProducer>'}}
  }
  {
    WeakPtr<DerivedProducer> derived_ptr =
        static_cast<WeakPtr<DerivedProducer> >(ptr);  // expected-error {{no matching conversion for static_cast from 'WeakPtr<Producer>' to 'WeakPtr<DerivedProducer>'}}
  }
}

void RefDowncastDisallowed() {
  Producer f;
  WeakPtr<Producer> ptr = f.AsWeakPtr();
  {
    WeakPtr<DerivedProducer>& derived_ptr = ptr;  // expected-error {{non-const lvalue reference to type 'WeakPtr<DerivedProducer>' cannot bind to a value of unrelated type 'WeakPtr<Producer>'}}
  }
  {
    WeakPtr<DerivedProducer>& derived_ptr =
        static_cast<WeakPtr<DerivedProducer>&>(ptr);  // expected-error {{non-const lvalue reference to type 'WeakPtr<DerivedProducer>' cannot bind to a value of unrelated type 'WeakPtr<Producer>'}}
  }
}

void VendingMutablePtrsFromConstFactoryDisallowed() {
  {
    Unrelated unrelated;
    const WeakPtrFactory<const Unrelated> factory(&unrelated);
    factory.GetMutableWeakPtr();  // expected-error {{invalid reference to function 'GetMutableWeakPtr': constraints not satisfied}}
  }
}

void UnauthorizedBindToCurrentSequenceDisallowed() {
  Unrelated unrelated;
  WeakPtrFactory<Unrelated> factory(&unrelated);
  factory.BindToCurrentSequence(
      subtle::BindWeakPtrFactoryPassKey());  // expected-error {{calling a private constructor of class 'base::subtle::BindWeakPtrFactoryPassKey'}}
}

}  // namespace base
