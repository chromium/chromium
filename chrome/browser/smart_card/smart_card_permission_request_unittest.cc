// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_permission_request.h"

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
const char kReaderName1[] = "reader1";
const char kReaderName2[] = "reader2";
const char kOrigin1[] = "https://example1.com";
const char kOrigin2[] = "https://example2.com";
}  // namespace

class SmartCardPermissionRequestTest : public testing::Test {
 public:
  void SetUp() override {
    origin1_ = url::Origin::Create(GURL(kOrigin1));
    origin2_ = url::Origin::Create(GURL(kOrigin2));
  }

 protected:
  url::Origin origin1_;
  url::Origin origin2_;
};

TEST_F(SmartCardPermissionRequestTest, IsDuplicateOf) {
  SmartCardPermissionRequest request1(origin1_, kReaderName1,
                                      base::DoNothing());
  SmartCardPermissionRequest request2(origin1_, kReaderName1,
                                      base::DoNothing());
  EXPECT_TRUE(request1.IsDuplicateOf(&request2));
}

TEST_F(SmartCardPermissionRequestTest, IsDuplicateOf_DifferentReader) {
  SmartCardPermissionRequest request1(origin1_, kReaderName1,
                                      base::DoNothing());
  SmartCardPermissionRequest request2(origin1_, kReaderName2,
                                      base::DoNothing());
  EXPECT_FALSE(request1.IsDuplicateOf(&request2));
}

TEST_F(SmartCardPermissionRequestTest, IsDuplicateOf_DifferentOrigin) {
  SmartCardPermissionRequest request1(origin1_, kReaderName1,
                                      base::DoNothing());
  SmartCardPermissionRequest request2(origin2_, kReaderName1,
                                      base::DoNothing());
  EXPECT_FALSE(request1.IsDuplicateOf(&request2));
}
