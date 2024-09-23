// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

TEST(RemoteHostContactedSignalTest, GeneratesUniqueIdForSignal) {
  RemoteHostContactedSignal signal = RemoteHostContactedSignal(
      /*extension_id=*/"ext-0",
      /*host_url=*/GURL("http://www.example.com/"),
      /*protocol=*/safe_browsing::RemoteHostInfo::HTTP_HTTPS,
      /*contact_initiator=*/safe_browsing::RemoteHostInfo::CONTENT_SCRIPT);
  EXPECT_EQ(signal.GetUniqueRemoteHostContactedId(),
            "www.example.com,HTTP_HTTPS,CONTENT_SCRIPT");
}

// TODO(crbug.com/40913716): Remove test once new RHC
// interception flow is fully launched.
TEST(RemoteHostContactedSignalTest,
     GeneratesUniqueIdForSignalWithDefaultContactInitiator) {
  RemoteHostContactedSignal signal = RemoteHostContactedSignal(
      /*extension_id=*/"ext-0",
      /*host_url=*/GURL("http://www.example.com/"),
      /*protocol=*/safe_browsing::RemoteHostInfo::WEBSOCKET);
  EXPECT_EQ(signal.GetUniqueRemoteHostContactedId(),
            "www.example.com,WEBSOCKET,EXTENSION");
}

}  // namespace

}  // namespace safe_browsing
