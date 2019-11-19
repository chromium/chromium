// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SCOPED_ACCOUNT_CONSISTENCY_H_
#define CHROME_BROWSER_SIGNIN_SCOPED_ACCOUNT_CONSISTENCY_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/public/base/account_consistency_method.h"

// Changes the account consistency method while it is in scope. Useful for
// tests.
class ScopedAccountConsistency {
 public:
  explicit ScopedAccountConsistency(signin::AccountConsistencyMethod method);
  ~ScopedAccountConsistency();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAccountConsistency);
};

// Specialized helper classes for each account consistency method:
// ScopedAccountConsistencyDice, ScopedAccountConsistencyMirror, ...

#define SCOPED_ACCOUNT_CONSISTENCY_SPECIALIZATION(method)                     \
  class ScopedAccountConsistency##method {                                    \
   public:                                                                    \
    ScopedAccountConsistency##method()                                        \
        : scoped_consistency_(signin::AccountConsistencyMethod::k##method) {} \
                                                                              \
   private:                                                                   \
    ScopedAccountConsistency scoped_consistency_;                             \
    DISALLOW_COPY_AND_ASSIGN(ScopedAccountConsistency##method);               \
  }

// ScopedAccountConsistencyMirror:
SCOPED_ACCOUNT_CONSISTENCY_SPECIALIZATION(Mirror);
// ScopedAccountConsistencyDiceMigration:
SCOPED_ACCOUNT_CONSISTENCY_SPECIALIZATION(DiceMigration);
// ScopedAccountConsistencyDice:
SCOPED_ACCOUNT_CONSISTENCY_SPECIALIZATION(Dice);

#undef SCOPED_ACCOUNT_CONSISTENCY_SPECIALIZATION

#endif  // CHROME_BROWSER_SIGNIN_SCOPED_ACCOUNT_CONSISTENCY_H_
