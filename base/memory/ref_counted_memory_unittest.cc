// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/memory/ref_counted_memory.h"

#include <stdint.h>

#include <utility>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace base {

TEST(RefCountedMemoryUnitTest, RefCountedStaticMemory) {
  {
    auto mem = MakeRefCounted<RefCountedStaticMemory>(
        byte_span_from_cstring("static mem"));

    EXPECT_THAT(span(*mem),
                ElementsAre('s', 't', 'a', 't', 'i', 'c', ' ', 'm', 'e', 'm'));
  }
}

TEST(RefCountedMemoryUnitTest, RefCountedBytes) {
  std::vector<uint8_t> data;
  data.push_back(45);
  data.push_back(99);
  scoped_refptr<RefCountedMemory> mem =
      MakeRefCounted<RefCountedBytes>(std::move(data));

  EXPECT_THAT(span(*mem), ElementsAre(45, 99));

  scoped_refptr<RefCountedMemory> mem2;
  {
    const uint8_t kData[] = {12, 11, 99};
    mem2 = MakeRefCounted<RefCountedBytes>(span(kData));
  }

  EXPECT_THAT(span(*mem2), ElementsAre(12, 11, 99));
}

TEST(RefCountedMemoryUnitTest, RefCountedBytesMutable) {
  auto mem = MakeRefCounted<RefCountedBytes>(10);

  EXPECT_THAT(span(*mem), ElementsAre(0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

  // Test non-const version of as_vector().
  mem->as_vector()[1u] = 1;

  EXPECT_THAT(span(*mem), ElementsAre(0, 1, 0, 0, 0, 0, 0, 0, 0, 0));
}

TEST(RefCountedMemoryUnitTest, RefCountedString) {
  scoped_refptr<RefCountedMemory> mem =
      MakeRefCounted<RefCountedString>(std::string("destroy me"));

  EXPECT_EQ(span(*mem), span_from_cstring("destroy me"));
}

TEST(RefCountedMemoryUnitTest, Equals) {
  scoped_refptr<RefCountedMemory> mem1 =
      MakeRefCounted<RefCountedString>(std::string("same"));

  std::vector<uint8_t> d2 = {'s', 'a', 'm', 'e'};
  scoped_refptr<RefCountedMemory> mem2 =
      MakeRefCounted<RefCountedBytes>(std::move(d2));

  EXPECT_TRUE(mem1->Equals(mem2));

  std::string s3("diff");
  scoped_refptr<RefCountedMemory> mem3 =
      MakeRefCounted<RefCountedString>(std::move(s3));

  EXPECT_FALSE(mem1->Equals(mem3));
  EXPECT_FALSE(mem2->Equals(mem3));
}

TEST(RefCountedMemoryUnitTest, EqualsNull) {
  std::string s("str");
  scoped_refptr<RefCountedMemory> mem =
      MakeRefCounted<RefCountedString>(std::move(s));
  EXPECT_FALSE(mem->Equals(nullptr));
}

}  //  namespace base
