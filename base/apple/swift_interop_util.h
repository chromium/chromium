// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_APPLE_SWIFT_INTEROP_UTIL_H_
#define BASE_APPLE_SWIFT_INTEROP_UTIL_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

#if __swift__
#include <swift/bridging>
#endif  // __swift__

// Class Wrappers
// ==============
//
// The following macros create a wrapper class that adds explicit constructors
// and destructors to work around an issue in the swift compiler where C++
// types are treated as POD when they have `= default` implementations. The
// wrappers also have the side-effect of instantiating templates when the
// wrapped type is a template instance, which is convenient for swift interop.
//
// Example:
// --------
//
// In the .h file:
//
//   SWIFT_DECLARE_INTEROP_WRAPPER(
//       IntCallback,
//       base::RepeatingCallback<void(int)>)
//
//   class ProgressObserver {
//    public:
//     IntCallback GetUpdateProgressCallback();
//    private:
//     void UpdateProgress(int progress);
//     base::WeakPtrFactory<MyClass> weak_ptr_factory_;
//   };
//
// In the .cc file:
//
//   SWIFT_DEFINE_INTEROP_WRAPPER(
//       IntCallback,
//       base::RepeatingCallback<void(int)>)
//
//   void ProgressObserver::UpdateProgress(int progress) {
//     // Do something with `progress`.
//   }
//   IntCallback ProgressObserver::GetUpdateProgressCallback() {
//     // Note: base::RepeatingCallback<void(int)> is implicitly cast to
//     // IntCallback
//     return base::BindRepeating(&ProgressObserver::UpdateProgress,
//         weak_ptr_factory_.GetWeakPtr());
//   }
//
// In the .swift file:
//
//   func downloadFile(progressCallback: IntCallback) async -> Data {
//     (...)
//     while(!done) {
//       (...)
//       progressCallback.Run(progress)
//     }
//   }

#define SWIFT_DECLARE_INTEROP_WRAPPER(NAME, WRAPPED_CLASS) \
  class NAME : public WRAPPED_CLASS {                      \
   public:                                                 \
    NAME();                                                \
    NAME(const NAME&);                                     \
    NAME(const WRAPPED_CLASS&);                            \
    NAME(NAME&&);                                          \
    NAME(WRAPPED_CLASS&&);                                 \
    ~NAME();                                               \
  };

#define SWIFT_DEFINE_INTEROP_WRAPPER(NAME, WRAPPED_CLASS)                \
  NAME::NAME() {}                                                        \
  NAME::NAME(const NAME& other) : WRAPPED_CLASS(other) {}                \
  NAME::NAME(const WRAPPED_CLASS& other) : WRAPPED_CLASS(other) {}       \
  NAME::NAME(NAME&& other) : WRAPPED_CLASS(std::move(other)) {}          \
  NAME::NAME(WRAPPED_CLASS&& other) : WRAPPED_CLASS(std::move(other)) {} \
  NAME::~NAME() {}

// The following are variations on SWIFT_{DECLARE|DEFINE}_INTEROP_WRAPPER
// but for move-only types, i.e. classes with deleted copy constructors, such
// as `base::OnceCallback`.
#define SWIFT_DECLARE_MOVE_ONLY_INTEROP_WRAPPER(NAME, WRAPPED_CLASS) \
  class NAME : public WRAPPED_CLASS {                                \
   public:                                                           \
    NAME();                                                          \
    NAME(NAME&&);                                                    \
    NAME(WRAPPED_CLASS&&);                                           \
    ~NAME();                                                         \
  };

#define SWIFT_DEFINE_MOVE_ONLY_INTEROP_WRAPPER(NAME, WRAPPED_CLASS)      \
  NAME::NAME() {}                                                        \
  NAME::NAME(NAME&& other) : WRAPPED_CLASS(std::move(other)) {}          \
  NAME::NAME(WRAPPED_CLASS&& other) : WRAPPED_CLASS(std::move(other)) {} \
  NAME::~NAME() {}

// Reference Type Helpers
// ======================
//
// The following are helper macros for bridging C++ classes that inherit
// base::RefCounted so that they can be safely used as reference types in
// swift.
//
// Example:
// --------
//
// In the .h file:
//
//   // This must appear above the class
//   SWIFT_DECLARE_REF_COUNTED_HELPERS(MyClass)
//
//   class MyClass : public base::RefCounted<MyClass> {
//    public:
//     MyClass();
//     static MyClass* MakeForSwift() SWIFT_RETURNS_RETAINED;
//    private:
//     friend class base::RefCounted<MyClass>;
//     ~MyClass();
//   } SWIFT_REF_COUNTED(MyClass);
//
// In the .cc file:
//
//   SWIFT_DEFINE_REF_COUNTED_HELPERS(MyClass)
//
//   MyClass* MyClass::MakeForSwift() {
//     return base::base::MakeRefCounted<MyClass>().release();
//   }
//
// In the .swift file:
//
//   let my_instance = MyClass.MakeForSwift()
//
// Notes:
// ------
//
// In order to move a reference from C++ to swift, as with a factory
// method, the C++ method returning the reference must:
//   1. Return a raw pointer of the ref counted type.
//   2. Generate the return value by leaking the pointer by calling
//      `.release()` on a base::scoped_refptr.
//   3. Have the `SWIFT_RETURNS_RETAINED` annotation, which tells the swift
//      compiler that the swift caller must adopt the leaked reference (i.e.
//      it will not increment the ref count when taking ownership).
//
// Best practice: Use the `ForSwift` suffix on methods that leak references
// in order to help prevent accidental memory leaks that may happen if such
// methods are called from C++.
//
// Methods that are meant to pass a ref counted object to swift without moving
// an existing reference:
//   1. Return a raw pointer of the ref counted type.
//   2. Generate the return value by calling `.get()` on a base::scoped_refptr
//   3. DO NOT annotate the method with `SWIFT_RETURNS_RETAINED`, so that
//      the swift compiler will generate code that acquires a new reference
//      (i.e. increments the ref count).

#define SWIFT_DECLARE_REF_COUNTED_HELPERS(REF_COUNTED_CLASS)    \
  class REF_COUNTED_CLASS;                                      \
  void Retain_##REF_COUNTED_CLASS(REF_COUNTED_CLASS* instance); \
  void Release_##REF_COUNTED_CLASS(REF_COUNTED_CLASS* instance);

#define SWIFT_DEFINE_REF_COUNTED_HELPERS(REF_COUNTED_CLASS)       \
  void Retain_##REF_COUNTED_CLASS(REF_COUNTED_CLASS* instance) {  \
    instance->AddRef();                                           \
  }                                                               \
  void Release_##REF_COUNTED_CLASS(REF_COUNTED_CLASS* instance) { \
    instance->Release();                                          \
  }

#if __swift__

#define SWIFT_REF_COUNTED(REF_COUNTED_CLASS)         \
  SWIFT_SHARED_REFERENCE(Retain_##REF_COUNTED_CLASS, \
                         Release_##REF_COUNTED_CLASS)

#else  // !__swift__

// Stubs for the C++ compiler.
#define SWIFT_REF_COUNTED(REF_COUNTED_CLASS)
#define SWIFT_RETURNS_RETAINED
#define SWIFT_SHARED_REFERENCE(RETAIN, RELEASE)

#endif  // !__swift__

#endif  // BASE_APPLE_SWIFT_INTEROP_UTIL_H_
