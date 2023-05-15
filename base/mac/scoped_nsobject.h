// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_NSOBJECT_H_
#define BASE_MAC_SCOPED_NSOBJECT_H_

// Include NSObject.h directly because Foundation.h pulls in many dependencies.
// (Approx 100k lines of code versus 1.5k for NSObject.h). scoped_nsobject gets
// singled out because it is most typically included from other header files.
#import <Foundation/NSObject.h>

#include <type_traits>
#include <utility>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_typeref.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
@class NSAutoreleasePool;
#endif

// Note that this uses the direct runtime interface to reference counting.
// https://clang.llvm.org/docs/AutomaticReferenceCounting.html#runtime-support
// This is so this can work when compiled for ARC. Annotations are used to lie
// about the effects of these calls so that ARC will not insert any reference
// count calls of its own and leave the maintenance of the reference count to
// this class.

extern "C" {
id objc_retain(__unsafe_unretained id value)
    __attribute__((ns_returns_not_retained));
void objc_release(__unsafe_unretained id value);
id objc_autorelease(__unsafe_unretained id value)
    __attribute__((ns_returns_not_retained));
}

namespace base {

// scoped_nsobject<> is patterned after std::unique_ptr<>, but maintains
// ownership of an NSObject subclass object.  Style deviations here are solely
// for compatibility with std::unique_ptr<>'s interface, with which everyone is
// already familiar.
//
// scoped_nsobject<> takes ownership of an object (in the constructor or in
// reset()) by taking over the caller's existing ownership claim.  The caller
// must own the object it gives to scoped_nsobject<>, and relinquishes an
// ownership claim to that object.  scoped_nsobject<> does not call -retain,
// callers have to call this manually if appropriate.
//
// scoped_nsprotocol<> has the same behavior as scoped_nsobject, but can be used
// with protocols.
//
// scoped_nsobject<> is not to be used for NSAutoreleasePools. For C++ code use
// NSAutoreleasePool; for Objective-C(++) code use @autoreleasepool instead. We
// check for bad uses of scoped_nsobject and NSAutoreleasePool at compile time
// with a template specialization (see below).
//
// If Automatic Reference Counting (aka ARC) is enabled then the ownership
// policy is not controllable by the user as ARC make it really difficult to
// transfer ownership (the reference passed to scoped_nsobject constructor is
// sunk by ARC, and a design that a function will maybe consume an argument
// based on a different argument isn't expressible with attributes). Due to
// that, the policy is always to |RETAIN| when using ARC.

namespace internal {

template <typename NST>
struct ScopedNSProtocolTraits {
  static NST InvalidValue() { return nil; }
  static NST Retain(__unsafe_unretained NST nst) { return objc_retain(nst); }
  static void Release(__unsafe_unretained NST nst) { objc_release(nst); }
};

}  // namespace internal

template <typename NST>
class scoped_nsprotocol
    : public ScopedTypeRef<NST, internal::ScopedNSProtocolTraits<NST>> {
 public:
  using Traits = internal::ScopedNSProtocolTraits<NST>;

#if !defined(__has_feature) || !__has_feature(objc_arc)
  explicit constexpr scoped_nsprotocol(
      NST object = Traits::InvalidValue(),
      base::scoped_policy::OwnershipPolicy policy = base::scoped_policy::ASSUME)
      : ScopedTypeRef<NST, Traits>(object, policy) {}
#else
  explicit constexpr scoped_nsprotocol(NST object = Traits::InvalidValue())
      : ScopedTypeRef<NST, Traits>(object, base::scoped_policy::RETAIN) {}
#endif

  scoped_nsprotocol(const scoped_nsprotocol<NST>& that)
      : ScopedTypeRef<NST, Traits>(that) {}

  template <typename NSR>
  explicit scoped_nsprotocol(const scoped_nsprotocol<NSR>& that_as_subclass)
      : ScopedTypeRef<NST, Traits>(that_as_subclass) {}

  scoped_nsprotocol(scoped_nsprotocol<NST>&& that)
      : ScopedTypeRef<NST, Traits>(std::move(that)) {}

  scoped_nsprotocol& operator=(const scoped_nsprotocol<NST>& that) {
    ScopedTypeRef<NST, Traits>::operator=(that);
    return *this;
  }

  void reset(const scoped_nsprotocol<NST>& that) {
    ScopedTypeRef<NST, Traits>::reset(that);
  }

#if !defined(__has_feature) || !__has_feature(objc_arc)
  void reset(NST object = Traits::InvalidValue(),
             base::scoped_policy::OwnershipPolicy policy =
                 base::scoped_policy::ASSUME) {
    ScopedTypeRef<NST, Traits>::reset(object, policy);
  }
#else
  void reset(NST object = Traits::InvalidValue()) {
    ScopedTypeRef<NST, Traits>::reset(object, base::scoped_policy::RETAIN);
  }
#endif

  // Shift reference to the autorelease pool to be released later.
  NST autorelease() __attribute__((ns_returns_not_retained)) {
    return objc_autorelease(this->release());
  }
};

// Free functions
template <class C>
void swap(scoped_nsprotocol<C>& p1, scoped_nsprotocol<C>& p2) {
  p1.swap(p2);
}

template <class C>
bool operator==(C p1, const scoped_nsprotocol<C>& p2) {
  return p1 == p2.get();
}

template <class C>
bool operator!=(C p1, const scoped_nsprotocol<C>& p2) {
  return p1 != p2.get();
}

template <typename NST>
class scoped_nsobject : public scoped_nsprotocol<NST*> {
 public:
  using Traits = typename scoped_nsprotocol<NST*>::Traits;

#if !defined(__has_feature) || !__has_feature(objc_arc)
  explicit constexpr scoped_nsobject(
      NST* object = Traits::InvalidValue(),
      base::scoped_policy::OwnershipPolicy policy = base::scoped_policy::ASSUME)
      : scoped_nsprotocol<NST*>(object, policy) {}
#else
  explicit constexpr scoped_nsobject(NST* object = Traits::InvalidValue())
      : scoped_nsprotocol<NST*>(object) {}
#endif

  scoped_nsobject(const scoped_nsobject<NST>& that)
      : scoped_nsprotocol<NST*>(that) {}

  template <typename NSR>
  explicit scoped_nsobject(const scoped_nsobject<NSR>& that_as_subclass)
      : scoped_nsprotocol<NST*>(that_as_subclass) {}

  scoped_nsobject(scoped_nsobject<NST>&& that)
      : scoped_nsprotocol<NST*>(std::move(that)) {}

  scoped_nsobject& operator=(const scoped_nsobject<NST>& that) {
    scoped_nsprotocol<NST*>::operator=(that);
    return *this;
  }

  void reset(const scoped_nsobject<NST>& that) {
    scoped_nsprotocol<NST*>::reset(that);
  }

#if !defined(__has_feature) || !__has_feature(objc_arc)
  void reset(NST* object = Traits::InvalidValue(),
             base::scoped_policy::OwnershipPolicy policy =
                 base::scoped_policy::ASSUME) {
    scoped_nsprotocol<NST*>::reset(object, policy);
  }
#else
  void reset(NST* object = Traits::InvalidValue()) {
    scoped_nsprotocol<NST*>::reset(object);
  }
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
  static_assert(std::is_same<NST, NSAutoreleasePool>::value == false,
                "Use @autoreleasepool instead");
#endif
};

// Specialization to make scoped_nsobject<id> work.
template<>
class scoped_nsobject<id> : public scoped_nsprotocol<id> {
 public:
  using Traits = typename scoped_nsprotocol<id>::Traits;

#if !defined(__has_feature) || !__has_feature(objc_arc)
  explicit constexpr scoped_nsobject(
      id object = Traits::InvalidValue(),
      base::scoped_policy::OwnershipPolicy policy = base::scoped_policy::ASSUME)
      : scoped_nsprotocol<id>(object, policy) {}
#else
  explicit constexpr scoped_nsobject(id object = Traits::InvalidValue())
      : scoped_nsprotocol<id>(object) {}
#endif

  scoped_nsobject(const scoped_nsobject<id>& that) = default;

  template <typename NSR>
  explicit scoped_nsobject(const scoped_nsobject<NSR>& that_as_subclass)
      : scoped_nsprotocol<id>(that_as_subclass) {}

  scoped_nsobject(scoped_nsobject<id>&& that)
      : scoped_nsprotocol<id>(std::move(that)) {}

  scoped_nsobject& operator=(const scoped_nsobject<id>& that) = default;

  void reset(const scoped_nsobject<id>& that) {
    scoped_nsprotocol<id>::reset(that);
  }

#if !defined(__has_feature) || !__has_feature(objc_arc)
  void reset(id object = Traits::InvalidValue(),
             base::scoped_policy::OwnershipPolicy policy =
                 base::scoped_policy::ASSUME) {
    scoped_nsprotocol<id>::reset(object, policy);
  }
#else
  void reset(id object = Traits::InvalidValue()) {
    scoped_nsprotocol<id>::reset(object);
  }
#endif
};

}  // namespace base

#endif  // BASE_MAC_SCOPED_NSOBJECT_H_
