// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/memory/self_deleting.h"

namespace base {

// 1. Class does not inherit from SelfDeleting.
class NotDerived {
 public:
  NotDerived(SelfDeletingPassKey) {}
 private:
  ~NotDerived() = default;
};

void MakeSelfDeletingNotDerived() {
  // expected-error@base/memory/self_deleting.h:* {{static assertion failed: Must be derived from base::SelfDeleting.}}
  MakeSelfDeleting<NotDerived>();
}

// 2. Class has a public destructor.
class PublicDestructor : public SelfDeleting {
 public:
  PublicDestructor(SelfDeletingPassKey key) : SelfDeleting(key) {}
  ~PublicDestructor() = default;
};

void MakeSelfDeletingPublicDestructor() {
  // expected-error-re@base/memory/self_deleting.h:* {{static assertion failed due to requirement '!std::destructible<base::PublicDestructor>'{{.*}}Destructor is public. Make it private to avoid misuse.}}
  MakeSelfDeleting<PublicDestructor>();
}

// 3. Class constructor is missing SelfDeletingPassKey as last parameter.
class WrongPassKeyPosition : public SelfDeleting {
 public:
  WrongPassKeyPosition(SelfDeletingPassKey key, int) : SelfDeleting(key) {}
 private:
  ~WrongPassKeyPosition() = default;
};

void MakeSelfDeletingWrongPassKeyPosition() {
  // expected-error@base/memory/self_deleting.h:* {{no matching constructor for initialization of 'base::WrongPassKeyPosition'}}
  MakeSelfDeleting<WrongPassKeyPosition>(1);
}

// 4. Direct construction with new is disallowed.
class ValidSelfDeleter : public SelfDeleting {
 public:
  ValidSelfDeleter(SelfDeletingPassKey key) : SelfDeleting(key) {}
 private:
  ~ValidSelfDeleter() = default;
};

void DirectConstructionDisallowed() {
  // Cannot construct without PassKey.
  new ValidSelfDeleter();  // expected-error {{no matching constructor for initialization of 'ValidSelfDeleter'}}

  // Cannot construct with PassKey because PassKey constructor is private.
  new ValidSelfDeleter(SelfDeletingPassKey());  // expected-error {{calling a private constructor of class 'base::PassKey<base::internal::MakeSelfDeletingImpl>'}}
}

}  // namespace base
