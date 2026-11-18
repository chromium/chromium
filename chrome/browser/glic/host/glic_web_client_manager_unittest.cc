// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_client_manager.h"

#include "base/process/kill.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

TEST(GlicWebClientManagerTest, ReasonForGuestProcessGoneMapsTerminationStatus) {
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_CRASHED),
            ClientLoadErrorReason::kGuestProcessCrashed);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_CRASHED),
            mojom::ClientLoadErrorReason::kGuestProcessCrashed);
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED),
            ClientLoadErrorReason::kGuestProcessKilled);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED),
            mojom::ClientLoadErrorReason::kGuestProcessKilled);
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM),
            ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM),
            mojom::ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_EVICTED_FOR_MEMORY),
            ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_EVICTED_FOR_MEMORY),
            mojom::ClientLoadErrorReason::kGuestProcessOom);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM),
            ClientLoadErrorReason::kGuestProcessOom);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM),
            mojom::ClientLoadErrorReason::kGuestProcessOom);
#endif
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM_PROTECTED),
            ClientLoadErrorReason::kGuestProcessGone);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_OOM_PROTECTED),
            mojom::ClientLoadErrorReason::kGuestProcessGone);
#endif
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_LAUNCH_FAILED),
            ClientLoadErrorReason::kGuestProcessLaunchFailed);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_LAUNCH_FAILED),
            mojom::ClientLoadErrorReason::kGuestProcessLaunchFailed);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_INTEGRITY_FAILURE),
            ClientLoadErrorReason::kGuestProcessIntegrityFailure);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_INTEGRITY_FAILURE),
            mojom::ClientLoadErrorReason::kGuestProcessIntegrityFailure);
#endif
  EXPECT_EQ(GlicWebClientManager::ReasonForGuestProcessGone(
                base::TERMINATION_STATUS_NORMAL_TERMINATION),
            ClientLoadErrorReason::kGuestProcessGone);
  EXPECT_EQ(GlicWebClientManager::MojoReasonForGuestProcessGone(
                base::TERMINATION_STATUS_ABNORMAL_TERMINATION),
            mojom::ClientLoadErrorReason::kGuestProcessGone);
}

}  // namespace glic
