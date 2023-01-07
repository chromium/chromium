// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/instance_id/instance_id_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

using extensions::ResultCatcher;

namespace extensions {

class InstanceIDApiTest : public ExtensionApiTest {
 public:
  InstanceIDApiTest();

  InstanceIDApiTest(const InstanceIDApiTest&) = delete;
  InstanceIDApiTest& operator=(const InstanceIDApiTest&) = delete;

 private:
  gcm::GCMProfileServiceFactory::ScopedTestingFactoryInstaller
      scoped_testing_factory_installer_;
};

InstanceIDApiTest::InstanceIDApiTest()
    : scoped_testing_factory_installer_(
          base::BindRepeating(&gcm::FakeGCMProfileService::Build)) {}

IN_PROC_BROWSER_TEST_F(InstanceIDApiTest, GetID) {
  ASSERT_TRUE(RunExtensionTest("instance_id/get_id"));
}

IN_PROC_BROWSER_TEST_F(InstanceIDApiTest, GetCreationTime) {
  ASSERT_TRUE(RunExtensionTest("instance_id/get_creation_time"));
}

IN_PROC_BROWSER_TEST_F(InstanceIDApiTest, DeleteID) {
  ASSERT_TRUE(RunExtensionTest("instance_id/delete_id"));
}

IN_PROC_BROWSER_TEST_F(InstanceIDApiTest, GetToken) {
  ASSERT_TRUE(RunExtensionTest("instance_id/get_token"));
}

IN_PROC_BROWSER_TEST_F(InstanceIDApiTest, DeleteToken) {
  ASSERT_TRUE(RunExtensionTest("instance_id/delete_token"));
}

IN_PROC_BROWSER_TEST_F(InstanceIDApiTest, Incognito) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  ResultCatcher incognito_catcher;
  incognito_catcher.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  ASSERT_TRUE(RunExtensionTest("instance_id/incognito", {},
                               {.allow_in_incognito = true}));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(incognito_catcher.GetNextResult()) << incognito_catcher.message();
}

}  // namespace extensions
