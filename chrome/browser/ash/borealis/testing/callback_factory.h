// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_CALLBACK_FACTORY_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_CALLBACK_FACTORY_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {

// A Factory object that creates callbacks with signature |F|. Deleting this
// object invalidates all callbacks created by it.
//
// The callbacks created by this factory are 'naggy' in the sense that
// uninteresting calls on it will cause a warning to be printed.
//
// Prefer to use the NiceCallbackFactory where possible (see
// go/gmock-cookbook#NiceStrictNaggy)
template <typename F>
class NaggyCallbackFactory : public ::testing::MockFunction<F> {
 public:
  // Create a callback object which will call the MockFunction's Call() method
  // at most once.
  base::OnceCallback<F> BindOnce() {
    return base::BindOnce(&NaggyCallbackFactory<F>::Call,
                          weak_factory_.GetWeakPtr());
  }

  // Create a callback object which will call the MockFunction's Call() method
  // zero or more times.
  base::RepeatingCallback<F> BindRepeating() {
    return base::BindRepeating(&NaggyCallbackFactory<F>::Call,
                               weak_factory_.GetWeakPtr());
  }

 private:
  base::WeakPtrFactory<NaggyCallbackFactory<F>> weak_factory_{this};
};

// As above, but uninteresting calls will be errors.
//
// Prefer to use the NiceCallbackFactory where possible (see
// go/gmock-cookbook#NiceStrictNaggy)
template <typename F>
using StrictCallbackFactory = ::testing::StrictMock<NaggyCallbackFactory<F>>;

// As above, but uninteresting calls will be ignored.
//
// We recommend using this factory in preference to the others, see
// go/gmock-cookbook#NiceStrictNaggy for more information.
template <typename F>
using NiceCallbackFactory = ::testing::NiceMock<NaggyCallbackFactory<F>>;

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_CALLBACK_FACTORY_H_
