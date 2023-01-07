// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class RunLoop;
}

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

template <typename T>
struct TypeTraits {
  typedef T ParamType;
};

template <>
struct TypeTraits<storage::FileSystemURL> {
  typedef const storage::FileSystemURL& ParamType;
};

template <typename T>
struct TypeTraits<std::vector<T> > {
  typedef const std::vector<T>& ParamType;
};

template <typename Arg1, typename Arg2, typename Param1, typename Param2>
void ReceiveResult2(bool* done,
                    Arg1* arg1_out,
                    Arg2* arg2_out,
                    Param1 arg1,
                    Param2 arg2) {
  EXPECT_FALSE(*done);
  *done = true;
  *arg1_out = std::forward<Param1>(arg1);
  *arg2_out = std::forward<Param2>(arg2);
}

template <typename R>
void AssignAndQuit(base::RunLoop* run_loop, R* result_out, R result);

template <typename R>
base::OnceCallback<void(R)> AssignAndQuitCallback(base::RunLoop* run_loop,
                                                  R* result);

template <typename Arg>
base::OnceCallback<void(typename TypeTraits<Arg>::ParamType)>
CreateResultReceiver(Arg* arg_out);

template <typename Arg1, typename Arg2>
base::OnceCallback<void(typename TypeTraits<Arg1>::ParamType,
                        typename TypeTraits<Arg2>::ParamType)>
CreateResultReceiver(Arg1* arg1_out, Arg2* arg2_out) {
  using Param1 = typename TypeTraits<Arg1>::ParamType;
  using Param2 = typename TypeTraits<Arg2>::ParamType;
  return base::BindOnce(&ReceiveResult2<Arg1, Arg2, Param1, Param2>,
                        base::Owned(new bool(false)), arg1_out, arg2_out);
}

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_TEST_UTIL_H_
