// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/type_tool_request.h"

#include <cstdint>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/shared_types.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

constexpr char kDocumentIdentifier[] = "doc_id";
constexpr int32_t kFakeActorSurfaceId = 123;

ActorSurfaceHandle NonNullSurfaceHandleForRequestPolicyTest() {
  return ActorSurfaceHandle::FromRawValue(kFakeActorSurfaceId);
}

PageTarget NonRootDomNodeTarget() {
  return DomNode{.node_id = 456, .document_identifier = kDocumentIdentifier};
}

TEST(TypeToolRequestTest, GetObservationPageStabilityConfig_Default) {
  TypeToolRequest request(NonNullSurfaceHandleForRequestPolicyTest(),
                          NonRootDomNodeTarget(), "hello",
                          /*follow_by_enter=*/false,
                          TypeToolRequest::Mode::kAppend);
  auto config = request.GetObservationPageStabilityConfig();
  EXPECT_TRUE(config.supports_paint_stability);
  EXPECT_EQ(config.start_delay, base::Seconds(1));
}

TEST(TypeToolRequestTest, GetObservationPageStabilityConfig_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kActorTypeToolObservationStartDelay, {{"start_delay", "2s"}});

  TypeToolRequest request(NonNullSurfaceHandleForRequestPolicyTest(),
                          NonRootDomNodeTarget(), "hello",
                          /*follow_by_enter=*/false,
                          TypeToolRequest::Mode::kAppend);
  auto config = request.GetObservationPageStabilityConfig();
  EXPECT_TRUE(config.supports_paint_stability);
  EXPECT_EQ(config.start_delay, base::Seconds(2));
}

TEST(TypeToolRequestTest, GetObservationPageStabilityConfig_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorTypeToolObservationStartDelay);

  TypeToolRequest request(NonNullSurfaceHandleForRequestPolicyTest(),
                          NonRootDomNodeTarget(), "hello",
                          /*follow_by_enter=*/false,
                          TypeToolRequest::Mode::kAppend);
  auto config = request.GetObservationPageStabilityConfig();
  EXPECT_TRUE(config.supports_paint_stability);
  EXPECT_EQ(config.start_delay, base::TimeDelta());
}

}  // namespace
}  // namespace actor
