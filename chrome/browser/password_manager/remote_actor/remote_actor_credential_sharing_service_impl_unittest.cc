// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/remote_actor/remote_actor_credential_sharing_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(RemoteActorCredentialSharingServiceTest, FactoryCreatesInstance) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kRemoteActorCredentialSharing);

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  RemoteActorCredentialSharingService* service =
      RemoteActorCredentialSharingServiceFactory::GetForProfile(&profile);
  EXPECT_NE(service, nullptr);
}

}  // namespace password_manager
