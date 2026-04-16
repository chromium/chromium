// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/metrics_types.h"

#include "chrome/browser/glic/host/glic.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

TEST(MetricsTypesTest, GetInvocationSourceString) {
  EXPECT_EQ("ExperimentalTriggering",
            GetInvocationSourceString(
                mojom::InvocationSource::kExperimentalTriggering));
  EXPECT_EQ("UniversalCart",
            GetInvocationSourceString(mojom::InvocationSource::kUniversalCart));
}

TEST(MetricsTypesTest, GetResponseSegmentation) {
  // Attached Text
  EXPECT_EQ(ResponseSegmentation::kExperimentalTriggeringAttachedText,
            GetResponseSegmentation(
                /*attached=*/true, mojom::WebClientMode::kText,
                mojom::InvocationSource::kExperimentalTriggering));

  // Attached Audio
  EXPECT_EQ(ResponseSegmentation::kExperimentalTriggeringAttachedAudio,
            GetResponseSegmentation(
                /*attached=*/true, mojom::WebClientMode::kAudio,
                mojom::InvocationSource::kExperimentalTriggering));

  // Detached Text
  EXPECT_EQ(ResponseSegmentation::kExperimentalTriggeringDetachedText,
            GetResponseSegmentation(
                /*attached=*/false, mojom::WebClientMode::kText,
                mojom::InvocationSource::kExperimentalTriggering));

  // Detached Audio
  EXPECT_EQ(ResponseSegmentation::kExperimentalTriggeringDetachedAudio,
            GetResponseSegmentation(
                /*attached=*/false, mojom::WebClientMode::kAudio,
                mojom::InvocationSource::kExperimentalTriggering));
}

}  // namespace
}  // namespace glic
