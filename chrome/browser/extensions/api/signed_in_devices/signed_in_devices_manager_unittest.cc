// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_manager.h"

#include <memory>

#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/extensions/api/signed_in_devices.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// Adds a listener and removes it.
TEST(SignedInDevicesManager, UpdateListener) {
  content::BrowserTaskEnvironment task_environment;

  std::unique_ptr<TestingProfile> profile =
      IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment();
  IdentityTestEnvironmentProfileAdaptor adaptor(profile.get());
  adaptor.identity_test_env()->SetPrimaryAccount("foo@test.com");

  ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile.get(), BrowserContextKeyedServiceFactory::TestingFactory());
  SignedInDevicesManager manager(profile.get());

  EventListenerInfo info(api::signed_in_devices::OnDeviceInfoChange::kEventName,
                         "extension1", GURL(), profile.get());

  // Add a listener.
  manager.OnListenerAdded(info);
  EXPECT_EQ(manager.change_observers_.size(), 1U);
  EXPECT_EQ(manager.change_observers_[0]->extension_id(), info.extension_id);

  // Remove the listener.
  manager.OnListenerRemoved(info);
  EXPECT_TRUE(manager.change_observers_.empty());
}
}  // namespace extensions
