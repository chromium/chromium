// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCOPED_SINGLETON_RESETTER_FOR_TEST_H_
#define ASH_PUBLIC_CPP_SCOPED_SINGLETON_RESETTER_FOR_TEST_H_

#include "base/memory/raw_ptr.h"

namespace ash {

// Helper class to reset singleton instance in constructor and restore it in
// destructor so that tests could create its own instance.
template <typename T>
class ASH_PUBLIC_EXPORT ScopedSingletonResetterForTest {
 public:
  ScopedSingletonResetterForTest() : instance_(GetGlobalInstanceHolder()) {
    GetGlobalInstanceHolder() = nullptr;
  }

  ~ScopedSingletonResetterForTest() { GetGlobalInstanceHolder() = instance_; }

 private:
  // Override this method to provide pointer holding singleton instance.
  T*& GetGlobalInstanceHolder();
  const raw_ptr<T> instance_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCOPED_SINGLETON_RESETTER_FOR_TEST_H_
