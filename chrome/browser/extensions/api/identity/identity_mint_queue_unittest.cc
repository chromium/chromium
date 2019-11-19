// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"

#include <memory>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionTokenKey;
using extensions::IdentityMintRequestQueue;

namespace {

class MockRequest : public extensions::IdentityMintRequestQueue::Request {
 public:
  MOCK_METHOD1(StartMintToken, void(IdentityMintRequestQueue::MintType));
};

std::unique_ptr<ExtensionTokenKey> ExtensionIdToKey(
    const std::string& extension_id) {
  return std::unique_ptr<ExtensionTokenKey>(new ExtensionTokenKey(
      extension_id, CoreAccountId("user_id"), std::set<std::string>()));
}

}  // namespace

TEST(IdentityMintQueueTest, SerialRequests) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::unique_ptr<ExtensionTokenKey> key(ExtensionIdToKey("ext_id"));
  MockRequest request1;
  MockRequest request2;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, *key, &request1);
  queue.RequestComplete(type, *key, &request1);

  EXPECT_CALL(request2, StartMintToken(type)).Times(1);
  queue.RequestStart(type, *key, &request2);
  queue.RequestComplete(type, *key, &request2);
}

TEST(IdentityMintQueueTest, InteractiveType) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;
  IdentityMintRequestQueue queue;
  std::unique_ptr<ExtensionTokenKey> key(ExtensionIdToKey("ext_id"));
  MockRequest request1;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, *key, &request1);
  queue.RequestComplete(type, *key, &request1);
}

TEST(IdentityMintQueueTest, ParallelRequests) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::unique_ptr<ExtensionTokenKey> key(ExtensionIdToKey("ext_id"));
  MockRequest request1;
  MockRequest request2;
  MockRequest request3;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, *key, &request1);
  queue.RequestStart(type, *key, &request2);
  queue.RequestStart(type, *key, &request3);

  EXPECT_CALL(request2, StartMintToken(type)).Times(1);
  queue.RequestComplete(type, *key, &request1);

  EXPECT_CALL(request3, StartMintToken(type)).Times(1);
  queue.RequestComplete(type, *key, &request2);

  queue.RequestComplete(type, *key, &request3);
}

TEST(IdentityMintQueueTest, ParallelRequestsFromTwoKeys) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::unique_ptr<ExtensionTokenKey> key1(ExtensionIdToKey("ext_id_1"));
  std::unique_ptr<ExtensionTokenKey> key2(ExtensionIdToKey("ext_id_2"));
  MockRequest request1;
  MockRequest request2;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  EXPECT_CALL(request2, StartMintToken(type)).Times(1);
  queue.RequestStart(type, *key1, &request1);
  queue.RequestStart(type, *key2, &request2);

  queue.RequestComplete(type, *key1, &request1);
  queue.RequestComplete(type, *key2, &request2);
}

TEST(IdentityMintQueueTest, Empty) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_INTERACTIVE;
  IdentityMintRequestQueue queue;
  std::unique_ptr<ExtensionTokenKey> key(ExtensionIdToKey("ext_id"));
  MockRequest request1;

  EXPECT_TRUE(queue.empty(type, *key));
  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, *key, &request1);
  EXPECT_FALSE(queue.empty(type, *key));
  queue.RequestComplete(type, *key, &request1);
  EXPECT_TRUE(queue.empty(type, *key));
}

TEST(IdentityMintQueueTest, Cancel) {
  IdentityMintRequestQueue::MintType type =
      IdentityMintRequestQueue::MINT_TYPE_NONINTERACTIVE;
  IdentityMintRequestQueue queue;
  std::unique_ptr<ExtensionTokenKey> key(ExtensionIdToKey("ext_id"));
  MockRequest request1;
  MockRequest request2;
  MockRequest request3;
  MockRequest request4;

  EXPECT_CALL(request1, StartMintToken(type)).Times(1);
  queue.RequestStart(type, *key, &request1);
  queue.RequestStart(type, *key, &request2);
  queue.RequestStart(type, *key, &request3);

  // request1: cancel the running head
  // request3: cancel a request that is not the head
  // request2: cancel the last request
  // request4: cancel a request that is not in the queue at all
  queue.RequestCancel(*key, &request1);
  queue.RequestCancel(*key, &request3);
  queue.RequestCancel(*key, &request2);
  queue.RequestCancel(*key, &request4);
}
