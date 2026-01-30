// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSERTEST_BASE_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#include "components/supervised_user/core/browser/android/android_parental_controls.h"
#else
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#endif  // BUILDFLAG(IS_ANDROID)

#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

// Base class for supervised user browser tests. It offers a common
// scaffolding for supervised user browser tests across Desktop and Android,
// focusing on abstracting user-transition related details, and providing
// entry-points for interacting with the services typically owned by the
// supervised user.
//
// Tests based on this base intentionally lack "SupervisionMixin" support, as
// they typically ignore profile-specific configuration (and instead focus of
// supervised features).
//
// The main rationale for having this base class is to offer an opportunity to
// share tests common to both platforms - where implementations differ, but
// functionalities are expected to remain consistent.
class SupervisedUserBrowserTestBase :
#if BUILDFLAG(IS_ANDROID)
    public AndroidBrowserTest
#else
    public MixinBasedInProcessBrowserTest
#endif  // BUILDFLAG(IS_ANDROID)
{
 public:
#if BUILDFLAG(IS_ANDROID)
  struct AndroidParentalControlsState {
    // Android parental controls browser filter state (enabled or disabled).
    bool browser_filter = false;
    // Android parental controls search filter state (enabled or disabled).
    bool search_filter = false;
  };
#endif  // BUILDFLAG(IS_ANDROID)

  // Describes status of the supervised user just before creating the services.
  struct InitialSupervisedUserState {
#if BUILDFLAG(IS_ANDROID)
    AndroidParentalControlsState android_parental_controls;
#endif  // BUILDFLAG(IS_ANDROID)
    // Family Link parental controls initial state.
    bool family_link_parental_controls = false;
  };

 protected:
  SupervisedUserBrowserTestBase();
  ~SupervisedUserBrowserTestBase() override;

  // Replaces default factories with test doubles.
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override;

  SupervisedUserService* GetSupervisedUserService() const;
  SupervisedUserUrlFilteringService* GetSupervisedUserUrlFilteringService()
      const;
  // Returns a pointer to the mock url checker client (transitively)owned by the
  // supervised user service.
  MockUrlCheckerClient& GetMockUrlCheckerClient();

  // Sets the initial state of all services involved in the supervised user
  // experience.
  void SetInitialSupervisedUserState(InitialSupervisedUserState initial_state);

#if BUILDFLAG(IS_ANDROID)
  AndroidParentalControls& GetDeviceParentalControls();
#else
  DeviceParentalControls& GetDeviceParentalControls();
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  InitialSupervisedUserState initial_state_;
  MockUrlCheckerClient mock_url_checker_client_;
  // Whether fakes of browser context keyed services were set up. This is used
  // to assert that some configurations can only be set before the services are
  // created.
  bool browser_context_keyed_services_set_up_ = false;
};

}  // namespace supervised_user
#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSERTEST_BASE_H_
