// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests that protobuf hardening options are enabled. Currently, the only option
// available (and tested) is PROTOBUF_INTERNAL_BOUNDS_CHECK_MODE_ABORT.

#include "base/protobuf_hardening_test_support.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

TEST(ProtobufHardeningTest, RepeatedFieldOutOfBounds) {
  HardeningTestMessage message;

  EXPECT_DEATH_IF_SUPPORTED(message.repeated_field(4), "");
  EXPECT_DEATH_IF_SUPPORTED(message.set_repeated_field(4, 44), "");

  EXPECT_DEATH_IF_SUPPORTED(message.repeated_field()[4], "");
  EXPECT_DEATH_IF_SUPPORTED(message.repeated_field().Get(4), "");

  EXPECT_DEATH_IF_SUPPORTED((*message.mutable_repeated_field())[4], "");
  EXPECT_DEATH_IF_SUPPORTED(message.mutable_repeated_field()->Get(4), "");
  EXPECT_DEATH_IF_SUPPORTED(message.mutable_repeated_field()->Mutable(4), "");
}

TEST(ProtobufHardeningTest, RepeatedPtrFieldOutOfBounds) {
  HardeningTestMessage message;

  EXPECT_DEATH_IF_SUPPORTED(message.repeated_ptr_field(4), "");
  EXPECT_DEATH_IF_SUPPORTED(message.mutable_repeated_ptr_field(4), "");

  EXPECT_DEATH_IF_SUPPORTED(message.repeated_ptr_field()[4], "");
  EXPECT_DEATH_IF_SUPPORTED(message.repeated_ptr_field().Get(4), "");

  EXPECT_DEATH_IF_SUPPORTED((*message.mutable_repeated_ptr_field())[4], "");
  EXPECT_DEATH_IF_SUPPORTED(message.mutable_repeated_ptr_field()->Get(4), "");
  EXPECT_DEATH_IF_SUPPORTED(message.mutable_repeated_ptr_field()->Mutable(4),
                            "");
}

}  // namespace base::test
