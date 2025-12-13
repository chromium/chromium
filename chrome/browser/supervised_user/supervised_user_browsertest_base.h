// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSERTEST_BASE_H_

#include "chrome/test/base/android/android_browser_test.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {

class MockUrlCheckerClient : public ::safe_search_api::URLCheckerClient {
 public:
  MockUrlCheckerClient();
  ~MockUrlCheckerClient() override;

  MOCK_METHOD(void,
              CheckURL,
              (const GURL& url, ClientCheckCallback callback),
              (override));

  base::WeakPtr<MockUrlCheckerClient> GetWeakPtr();

 private:
  base::WeakPtrFactory<MockUrlCheckerClient> weak_ptr_factory_{this};
};

// Base class for supervised user Android browser tests. This class provides
// common functionality and members for supervised user Android browser tests,
// notably sets up the supervised user service with required fakes.
class SupervisedUserBrowserTestBase : public AndroidBrowserTest {
 protected:
  // Describes status of the supervised user just before creating the services.
  struct InitialSupervisedUserState {
    // ACP browser filter initial state.
    bool android_parental_controls_browser_filter = false;
    // ACP search filter initial state.
    bool android_parental_controls_search_filter = false;
    // Family Link parental controls initial state.
    bool family_link_parental_controls = false;
  };

  SupervisedUserBrowserTestBase();
  ~SupervisedUserBrowserTestBase() override;

  // Replaces default factories with test doubles.
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override;

  SupervisedUserService* GetSupervisedUserService() const;
  base::WeakPtr<ContentFiltersObserverBridge>
  GetSearchContentFiltersObserverWeakPtr() const;
  base::WeakPtr<ContentFiltersObserverBridge>
  GetBrowserContentFiltersObserverWeakPtr() const;
  // Returns a pointer to the mock url checker client (transitively)owned by the
  // supervised user service.
  MockUrlCheckerClient& GetMockUrlCheckerClient();

  // Sets the initial state of all services involved in the supervised user
  // experience.
  void SetInitialSupervisedUserState(InitialSupervisedUserState initial_state);

 private:
  // Builds a SupervisedUserService with fakes. See
  // "SetInitialSupervisedUserState" to customize its behavior.
  std::unique_ptr<KeyedService> BuildSupervisedUserService(
      content::BrowserContext* browser_context);

  InitialSupervisedUserState initial_state_;
  MockUrlCheckerClient mock_url_checker_client_;

  bool mocks_set_up_ = false;  // Whether mocks were set up already.
};

}  // namespace supervised_user
#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSERTEST_BASE_H_
