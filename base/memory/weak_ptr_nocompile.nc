// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/memory/weak_ptr.h"

namespace base {

struct Producer : SupportsWeakPtr<Producer> {};
struct DerivedProducer : Producer {};
struct OtherDerivedProducer : Producer {};
struct MultiplyDerivedProducer : Producer,
                                 SupportsWeakPtr<MultiplyDerivedProducer> {};
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

void AsWeakPtrDowncastDisallowed() {
  Producer f;
  WeakPtr<DerivedProducer> ptr =
      SupportsWeakPtr<Producer>::StaticAsWeakPtr<DerivedProducer>(&f);  // expected-error {{no matching function for call to 'StaticAsWeakPtr'}}
}

void UnsafeDowncastViaAsWeakPtrDisallowed() {
  Producer f;
  {
    WeakPtr<DerivedProducer> ptr = AsWeakPtr(&f);  // expected-error {{no viable conversion from 'WeakPtr<base::Producer>' to 'WeakPtr<DerivedProducer>'}}
  }
  {
    WeakPtr<DerivedProducer> ptr = AsWeakPtr<DerivedProducer>(&f);  // expected-error {{no matching function for call to 'AsWeakPtr'}}
  }
  {
    WeakPtr<DerivedProducer> ptr = AsWeakPtr<Producer>(&f);  // expected-error {{no viable conversion from 'WeakPtr<base::Producer>' to 'WeakPtr<DerivedProducer>'}}
  }
}

void UnsafeSidecastViaAsWeakPtrDisallowed() {
  DerivedProducer f;
  {
    WeakPtr<OtherDerivedProducer> ptr = AsWeakPtr(&f);  // expected-error {{no viable conversion from 'WeakPtr<base::DerivedProducer>' to 'WeakPtr<OtherDerivedProducer>'}}
  }
  {
    WeakPtr<OtherDerivedProducer> ptr = AsWeakPtr<DerivedProducer>(&f);  // expected-error {{no viable conversion from 'WeakPtr<base::DerivedProducer>' to 'WeakPtr<OtherDerivedProducer>'}}
  }
  {
    WeakPtr<OtherDerivedProducer> ptr = AsWeakPtr<OtherDerivedProducer>(&f);  // expected-error {{no matching function for call to 'AsWeakPtr'}}
  }
}

void UnrelatedCastViaAsWeakPtrDisallowed() {
  DerivedProducer f;
  {
    WeakPtr<Unrelated> ptr = AsWeakPtr(&f);  // expected-error {{no viable conversion from 'WeakPtr<base::DerivedProducer>' to 'WeakPtr<Unrelated>'}}
  }
  {
    WeakPtr<Unrelated> ptr = AsWeakPtr<Unrelated>(&f);  // expected-error {{no matching function for call to 'AsWeakPtr'}}
  }
}

void AsWeakPtrWithoutSupportsWeakPtrDisallowed() {
  {
    Unrelated f;
    WeakPtr<Unrelated> ptr = AsWeakPtr(&f);  // expected-error@*:* {{AsWeakPtr argument must inherit from SupportsWeakPtr}}
    // expected-error@*:* {{no viable constructor or deduction guide for deduction of template arguments of 'ExtractSinglyInheritedBase'}}
    // expected-error@*:* {{static_cast from 'base::Unrelated *' to 'SupportsWeakPtr<Base> *' (aka 'SupportsWeakPtr<int> *'), which are not related by inheritance, is not allowed}}
  }
  {
    DerivedUnrelated f;
    WeakPtr<Unrelated> ptr = AsWeakPtr(&f);  // expected-error@*:* {{AsWeakPtr argument must inherit from SupportsWeakPtr}}
    // expected-error@*:* {{no viable constructor or deduction guide for deduction of template arguments of 'ExtractSinglyInheritedBase'}}
    // expected-error@*:* {{static_cast from 'base::DerivedUnrelated *' to 'SupportsWeakPtr<Base> *' (aka 'SupportsWeakPtr<int> *'), which are not related by inheritance, is not allowed}}
  }
}

void AsWeakPtrWithAmbiguousAncestorsDisallowed() {
  MultiplyDerivedProducer f;
  WeakPtr<MultiplyDerivedProducer> ptr = AsWeakPtr(&f);  // expected-error@*:* {{no viable constructor or deduction guide for deduction of template arguments of 'ExtractSinglyInheritedBase'}}
  // expected-error@*:* {{static_cast from 'base::MultiplyDerivedProducer *' to 'SupportsWeakPtr<Base> *' (aka 'SupportsWeakPtr<int> *'), which are not related by inheritance, is not allowed}}
}

void VendingMutablePtrsFromConstFactoryDisallowed() {
  {
    Unrelated unrelated;
    const WeakPtrFactory<const Unrelated> factory(&unrelated);
    factory.GetMutableWeakPtr();  // expected-error {{no matching member function for call to 'GetMutableWeakPtr'}}
  }
  {
    DerivedUnrelated derived_unrelated;
    const WeakPtrFactory<DerivedUnrelated> factory(&derived_unrelated);
    factory.GetMutableWeakPtr<Unrelated>();  // expected-error {{no matching member function for call to 'GetMutableWeakPtr'}}
  }
}

}  // namespace base
