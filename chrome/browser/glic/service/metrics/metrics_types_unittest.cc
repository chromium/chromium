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
  EXPECT_EQ("PasswordChange", GetInvocationSourceString(
                                  mojom::InvocationSource::kPasswordChange));
  EXPECT_EQ(
      "DaisyChainOnNewTab",
      GetInvocationSourceString(mojom::InvocationSource::kDaisyChainOnNewTab));
  EXPECT_EQ("DaisyChainOnFollowLink",
            GetInvocationSourceString(
                mojom::InvocationSource::kDaisyChainOnFollowLink));
  EXPECT_EQ(
      "ConversationSwitch",
      GetInvocationSourceString(mojom::InvocationSource::kConversationSwitch));
  EXPECT_EQ(
      "DetachAttachButton",
      GetInvocationSourceString(mojom::InvocationSource::kDetachAttachButton));
  EXPECT_EQ("TabRestore",
            GetInvocationSourceString(mojom::InvocationSource::kTabRestore));
  EXPECT_EQ("ReshowInactive", GetInvocationSourceString(
                                  mojom::InvocationSource::kReshowInactive));
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

  // PasswordChange Attached Text
  EXPECT_EQ(ResponseSegmentation::kPasswordChangeAttachedText,
            GetResponseSegmentation(
                /*attached=*/true, mojom::WebClientMode::kText,
                mojom::InvocationSource::kPasswordChange));

  // PasswordChange Attached Audio
  EXPECT_EQ(ResponseSegmentation::kPasswordChangeAttachedAudio,
            GetResponseSegmentation(
                /*attached=*/true, mojom::WebClientMode::kAudio,
                mojom::InvocationSource::kPasswordChange));

  // PasswordChange Detached Text
  EXPECT_EQ(ResponseSegmentation::kPasswordChangeDetachedText,
            GetResponseSegmentation(
                /*attached=*/false, mojom::WebClientMode::kText,
                mojom::InvocationSource::kPasswordChange));

  // PasswordChange Detached Audio
  EXPECT_EQ(ResponseSegmentation::kPasswordChangeDetachedAudio,
            GetResponseSegmentation(
                /*attached=*/false, mojom::WebClientMode::kAudio,
                mojom::InvocationSource::kPasswordChange));

  // DaisyChainOnNewTab Attached Text
  EXPECT_EQ(ResponseSegmentation::kDaisyChainOnNewTabAttachedText,
            GetResponseSegmentation(
                /*attached=*/true, mojom::WebClientMode::kText,
                mojom::InvocationSource::kDaisyChainOnNewTab));

  // DaisyChainOnFollowLink Detached Audio
  EXPECT_EQ(ResponseSegmentation::kDaisyChainOnFollowLinkDetachedAudio,
            GetResponseSegmentation(
                /*attached=*/false, mojom::WebClientMode::kAudio,
                mojom::InvocationSource::kDaisyChainOnFollowLink));

  // ConversationSwitch Detached Audio
  EXPECT_EQ(ResponseSegmentation::kConversationSwitchDetachedAudio,
            GetResponseSegmentation(
                /*attached=*/false, mojom::WebClientMode::kAudio,
                mojom::InvocationSource::kConversationSwitch));

  // DetachAttachButton Attached Audio
  EXPECT_EQ(ResponseSegmentation::kDetachAttachButtonAttachedAudio,
            GetResponseSegmentation(
                /*attached=*/true, mojom::WebClientMode::kAudio,
                mojom::InvocationSource::kDetachAttachButton));

  // TabRestore Detached Text
  EXPECT_EQ(ResponseSegmentation::kTabRestoreDetachedText,
            GetResponseSegmentation(
                /*attached=*/false, mojom::WebClientMode::kText,
                mojom::InvocationSource::kTabRestore));

  // ReshowInactive Attached Text
  EXPECT_EQ(ResponseSegmentation::kReshowInactiveAttachedText,
            GetResponseSegmentation(
                /*attached=*/true, mojom::WebClientMode::kText,
                mojom::InvocationSource::kReshowInactive));
}

}  // namespace
}  // namespace glic
