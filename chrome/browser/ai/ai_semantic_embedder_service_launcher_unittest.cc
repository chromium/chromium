// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AISemanticEmbedderServiceLauncherForTest
    : public AISemanticEmbedderServiceLauncher {};

}  // namespace

class AISemanticEmbedderServiceLauncherTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AISemanticEmbedderServiceLauncherTest, InitiallyAllowedToLaunch) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  EXPECT_TRUE(launcher.AllowedToLaunch());
}

TEST_F(AISemanticEmbedderServiceLauncherTest,
       IdleDisconnectDoesNotCountAsCrash) {
  AISemanticEmbedderServiceLauncherForTest launcher;

  launcher.OnServiceDisconnected(/*is_idle=*/true);
  launcher.OnServiceDisconnected(/*is_idle=*/true);
  launcher.OnServiceDisconnected(/*is_idle=*/true);
  launcher.OnServiceDisconnected(/*is_idle=*/true);

  EXPECT_TRUE(launcher.AllowedToLaunch());
}

TEST_F(AISemanticEmbedderServiceLauncherTest, CrashDisconnectThrottlesLaunch) {
  AISemanticEmbedderServiceLauncherForTest launcher;

  launcher.OnServiceDisconnected(/*is_idle=*/false);
  EXPECT_TRUE(launcher.AllowedToLaunch());

  launcher.OnServiceDisconnected(/*is_idle=*/false);
  EXPECT_TRUE(launcher.AllowedToLaunch());

  launcher.OnServiceDisconnected(/*is_idle=*/false);
  EXPECT_FALSE(launcher.AllowedToLaunch());
}
