// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

TEST(BindObjcBlockTest, TestScopedClosureRunnerExitScope) {
  int run_count = 0;
  int* ptr = &run_count;
  {
    base::ScopedClosureRunner runner(base::BindOnce(^{
      (*ptr)++;
    }));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(BindObjcBlockTest, TestScopedClosureRunnerRelease) {
  int run_count = 0;
  int* ptr = &run_count;
  base::OnceClosure c;
  {
    base::ScopedClosureRunner runner(base::BindOnce(^{
      (*ptr)++;
    }));
    c = runner.Release();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(0, run_count);
  std::move(c).Run();
  EXPECT_EQ(1, run_count);
}

TEST(BindObjcBlockTest, TestReturnValue) {
  const int kReturnValue = 42;
  base::OnceCallback<int(void)> c = base::BindOnce(^{
    return kReturnValue;
  });
  EXPECT_EQ(kReturnValue, std::move(c).Run());
}

TEST(BindObjcBlockTest, TestArgument) {
  const int kArgument = 42;
  base::OnceCallback<int(int)> c = base::BindOnce(^(int a) {
    return a + 1;
  });
  EXPECT_EQ(kArgument + 1, std::move(c).Run(kArgument));
}

TEST(BindObjcBlockTest, TestTwoArguments) {
  std::string result;
  std::string* ptr = &result;
  base::OnceCallback<void(const std::string&, const std::string&)> c =
      base::BindOnce(^(const std::string& a, const std::string& b) {
        *ptr = a + b;
      });
  std::move(c).Run("forty", "two");
  EXPECT_EQ(result, "fortytwo");
}

TEST(BindObjcBlockTest, TestThreeArguments) {
  std::string result;
  std::string* ptr = &result;
  base::OnceCallback<void(const std::string&, const std::string&,
                          const std::string&)>
      cb = base::BindOnce(
          ^(const std::string& a, const std::string& b, const std::string& c) {
            *ptr = a + b + c;
          });
  std::move(cb).Run("six", "times", "nine");
  EXPECT_EQ(result, "sixtimesnine");
}

TEST(BindObjcBlockTest, TestSixArguments) {
  std::string result1;
  std::string* ptr = &result1;
  int result2;
  int* ptr2 = &result2;
  base::OnceCallback<void(int, int, const std::string&, const std::string&, int,
                          const std::string&)>
      cb = base::BindOnce(^(int a, int b, const std::string& c,
                            const std::string& d, int e, const std::string& f) {
        *ptr = c + d + f;
        *ptr2 = a + b + e;
      });
  std::move(cb).Run(1, 2, "infinite", "improbability", 3, "drive");
  EXPECT_EQ(result1, "infiniteimprobabilitydrive");
  EXPECT_EQ(result2, 6);
}

TEST(BindObjcBlockTest, TestBlockMoveable) {
  base::OnceClosure c;
  __block BOOL invoked_block = NO;
  @autoreleasepool {
    c = base::BindOnce(
        ^(std::unique_ptr<BOOL> v) {
          invoked_block = *v;
        },
        std::make_unique<BOOL>(YES));
  }
  std::move(c).Run();
  EXPECT_TRUE(invoked_block);
}

// Tests that the bound block is retained until the end of its execution, even
// if the callback itself is destroyed during the invocation. It was found that
// some code depends on this behaviour (see https://crbug.com/845687).
TEST(BindObjcBlockTest, TestBlockDeallocation) {
  base::RepeatingClosure closure;
  __block BOOL invoked_block = NO;
  closure = base::BindRepeating(
      ^(base::RepeatingClosure* this_closure) {
        *this_closure = base::RepeatingClosure();
        invoked_block = YES;
      },
      &closure);
  closure.Run();
  EXPECT_TRUE(invoked_block);
}

TEST(BindObjcBlockTest, TestBlockReleased) {
  __weak NSObject* weak_nsobject;
  @autoreleasepool {
    NSObject* nsobject = [[NSObject alloc] init];
    weak_nsobject = nsobject;

    auto callback = base::BindOnce(^{
      [nsobject description];
    });
  }
  EXPECT_NSEQ(nil, weak_nsobject);
}

}  // namespace
