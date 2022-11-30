// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tuple.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

void DoAdd(int a, int b, int c, int* res) {
  *res = a + b + c;
}

struct Addy {
  Addy() = default;
  void DoAdd(int a, int b, int c, int d, int* res) {
    *res = a + b + c + d;
  }
};

struct Addz {
  Addz() = default;
  void DoAdd(int a, int b, int c, int d, int e, int* res) {
    *res = a + b + c + d + e;
  }
};

}  // namespace

TEST(TupleTest, Basic) {
  std::tuple<int> t1(1);
  std::tuple<int, int, int, int*> t4(1, 2, 3, &std::get<0>(t1));
  std::tuple<int, int, int, int, int*> t5(1, 2, 3, 4, &std::get<0>(t4));
  std::tuple<int, int, int, int, int, int*> t6(1, 2, 3, 4, 5, &std::get<0>(t4));

  EXPECT_EQ(1, std::get<0>(t1));
  DispatchToFunction(&DoAdd, t4);
  EXPECT_EQ(6, std::get<0>(t1));

  int res = 0;
  DispatchToFunction(&DoAdd, std::make_tuple(9, 8, 7, &res));
  EXPECT_EQ(24, res);

  Addy addy;
  EXPECT_EQ(1, std::get<0>(t4));
  DispatchToMethod(&addy, &Addy::DoAdd, t5);
  EXPECT_EQ(10, std::get<0>(t4));

  Addz addz;
  EXPECT_EQ(10, std::get<0>(t4));
  DispatchToMethod(&addz, &Addz::DoAdd, t6);
  EXPECT_EQ(15, std::get<0>(t4));
}

namespace {

struct CopyLogger {
  CopyLogger() { ++TimesConstructed; }
  CopyLogger(const CopyLogger& tocopy) { ++TimesConstructed; ++TimesCopied; }
  ~CopyLogger() = default;

  static int TimesCopied;
  static int TimesConstructed;
};

void SomeLoggerMethRef(const CopyLogger& logy, const CopyLogger* ptr, bool* b) {
  *b = &logy == ptr;
}

void SomeLoggerMethCopy(CopyLogger logy, const CopyLogger* ptr, bool* b) {
  *b = &logy == ptr;
}

int CopyLogger::TimesCopied = 0;
int CopyLogger::TimesConstructed = 0;

}  // namespace

TEST(TupleTest, Copying) {
  CopyLogger logger;
  EXPECT_EQ(0, CopyLogger::TimesCopied);
  EXPECT_EQ(1, CopyLogger::TimesConstructed);

  bool res = false;

  // Creating the tuple should copy the class to store internally in the tuple.
  std::tuple<CopyLogger, CopyLogger*, bool*> tuple(logger, &logger, &res);
  std::get<CopyLogger*>(tuple) = &std::get<CopyLogger>(tuple);
  EXPECT_EQ(2, CopyLogger::TimesConstructed);
  EXPECT_EQ(1, CopyLogger::TimesCopied);

  // Our internal Logger and the one passed to the function should be the same.
  res = false;
  DispatchToFunction(&SomeLoggerMethRef, tuple);
  EXPECT_TRUE(res);
  EXPECT_EQ(2, CopyLogger::TimesConstructed);
  EXPECT_EQ(1, CopyLogger::TimesCopied);

  // Now they should be different, since the function call will make a copy.
  res = false;
  DispatchToFunction(&SomeLoggerMethCopy, tuple);
  EXPECT_FALSE(res);
  EXPECT_EQ(3, CopyLogger::TimesConstructed);
  EXPECT_EQ(2, CopyLogger::TimesCopied);
}

}  // namespace base
