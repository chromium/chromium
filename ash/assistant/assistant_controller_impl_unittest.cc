// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller_impl.h"

#include <map>
#include <memory>
#include <string>

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

// MockAssistantControllerObserver ---------------------------------------------

class MockAssistantControllerObserver : public AssistantControllerObserver {
 public:
  MockAssistantControllerObserver() = default;
  ~MockAssistantControllerObserver() override = default;

  // AssistantControllerObserver:
  MOCK_METHOD(void, OnAssistantControllerConstructed, (), (override));

  MOCK_METHOD(void, OnAssistantControllerDestroying, (), (override));

  MOCK_METHOD(void, OnAssistantReady, (), (override));

  MOCK_METHOD(void,
              OnDeepLinkReceived,
              (assistant::util::DeepLinkType type,
               (const std::map<std::string, std::string>& params)),
              (override));

  MOCK_METHOD(void,
              OnOpeningUrl,
              (const GURL& url, bool in_background, bool from_server),
              (override));

  MOCK_METHOD(void,
              OnUrlOpened,
              (const GURL& url, bool from_server),
              (override));
};

class MockAssistantUiModelObserver : public AssistantUiModelObserver {
 public:
  MockAssistantUiModelObserver() = default;
  ~MockAssistantUiModelObserver() override = default;

  MOCK_METHOD(void,
              OnUiVisibilityChanged,
              (AssistantVisibility new_visibility,
               AssistantVisibility old_visibility,
               std::optional<AssistantEntryPoint> entry_point,
               std::optional<AssistantExitPoint> exit_point),
              (override));
};

// MockNewWindowDelegate -------------------------------------------------------

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));

  MOCK_METHOD(void,
              OpenFeedbackPage,
              (NewWindowDelegate::FeedbackSource source,
               const std::string& description_template),
              (override));
};

// AssistantControllerImplTest -------------------------------------------------

class AssistantControllerImplTest : public AssistantAshTestBase {
 public:
  AssistantControllerImplTest() = default;

  AssistantController* controller() { return AssistantController::Get(); }
  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }
  const AssistantUiModel* ui_model() {
    return AssistantUiController::Get()->GetModel();
  }

 private:
  MockNewWindowDelegate new_window_delegate_;
};

// Same with `AssistantControllerImplTest` except that this class does not set
// up an active user in `SetUp`.
class AssistantControllerImplTestForStartUp
    : public AssistantControllerImplTest {
 public:
  AssistantControllerImplTestForStartUp() {
    set_up_active_user_in_test_set_up_ = false;
  }
};

}  // namespace

// Tests -----------------------------------------------------------------------

// Tests that AssistantController observers are notified of deep link received.
TEST_F(AssistantControllerImplTest, NotifiesDeepLinkReceived) {
  testing::NiceMock<MockAssistantControllerObserver> controller_observer_mock;
  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      scoped_controller_obs{&controller_observer_mock};
  scoped_controller_obs.Observe(controller());

  EXPECT_CALL(controller_observer_mock, OnDeepLinkReceived)
      .WillOnce(
          testing::Invoke([](assistant::util::DeepLinkType type,
                             const std::map<std::string, std::string>& params) {
            EXPECT_EQ(assistant::util::DeepLinkType::kQuery, type);
            EXPECT_EQ("weather",
                      assistant::util::GetDeepLinkParam(
                          params, assistant::util::DeepLinkParam::kQuery)
                          .value());
          }));

  controller()->OpenUrl(
      assistant::util::CreateAssistantQueryDeepLink("weather"));
}

// Tests that AssistantController observers are notified of URLs opening and
// having been opened. Note that it is important that these events be notified
// before and after the URL is actually opened respectively.
TEST_F(AssistantControllerImplTest, NotifiesOpeningUrlAndUrlOpened) {
  testing::NiceMock<MockAssistantControllerObserver> controller_observer_mock;
  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      scoped_controller_obs{&controller_observer_mock};
  scoped_controller_obs.Observe(controller());

  // Enforce ordering of events.
  testing::InSequence sequence;

  EXPECT_CALL(controller_observer_mock, OnOpeningUrl)
      .WillOnce(testing::Invoke(
          [](const GURL& url, bool in_background, bool from_server) {
            EXPECT_EQ(GURL("https://g.co/"), url);
            EXPECT_TRUE(in_background);
            EXPECT_TRUE(from_server);
          }));

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL("https://g.co/"),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));

  EXPECT_CALL(controller_observer_mock, OnUrlOpened)
      .WillOnce(testing::Invoke([](const GURL& url, bool from_server) {
        EXPECT_EQ(GURL("https://g.co/"), url);
        EXPECT_TRUE(from_server);
      }));

  controller()->OpenUrl(GURL("https://g.co/"), /*in_background=*/true,
                        /*from_server=*/true);
}

TEST_F(AssistantControllerImplTest, NotOpenUrlIfAssistantNotReady) {
  testing::NiceMock<MockAssistantControllerObserver> controller_observer_mock;
  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      scoped_controller_obs{&controller_observer_mock};
  scoped_controller_obs.Observe(controller());

  EXPECT_CALL(controller_observer_mock, OnOpeningUrl).Times(0);

  controller()->SetAssistant(nullptr);
  controller()->OpenUrl(GURL("https://g.co/"), /*in_background=*/true,
                        /*from_server=*/true);
}

TEST_F(AssistantControllerImplTest, OpensFeedbackPageForFeedbackDeeplink) {
  testing::NiceMock<MockAssistantControllerObserver> controller_observer_mock;
  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      scoped_controller_obs{&controller_observer_mock};
  scoped_controller_obs.Observe(controller());

  EXPECT_CALL(controller_observer_mock, OnDeepLinkReceived)
      .WillOnce(
          testing::Invoke([](assistant::util::DeepLinkType type,
                             const std::map<std::string, std::string>& params) {
            EXPECT_EQ(assistant::util::DeepLinkType::kFeedback, type);
            std::map<std::string, std::string> expected_params;
            EXPECT_EQ(params, expected_params);
          }));

  EXPECT_CALL(new_window_delegate(), OpenFeedbackPage)
      .WillOnce([](NewWindowDelegate::FeedbackSource source,
                   const std::string& description_template) {
        EXPECT_EQ(NewWindowDelegate::FeedbackSource::kFeedbackSourceAssistant,
                  source);
        EXPECT_EQ(std::string(), description_template);
      });

  controller()->OpenUrl(GURL("googleassistant://send-feedback"),
                        /*in_background=*/false, /*from_server=*/true);
}

TEST_F(AssistantControllerImplTest, ClosesAssistantUiForFeedbackDeeplink) {
  ShowAssistantUi();

  testing::NiceMock<MockAssistantUiModelObserver> ui_model_observer_mock;
  ui_model()->AddObserver(&ui_model_observer_mock);

  testing::InSequence sequence;
  EXPECT_CALL(ui_model_observer_mock, OnUiVisibilityChanged)
      .WillOnce([](AssistantVisibility new_visibility,
                   AssistantVisibility old_visibility,
                   std::optional<AssistantEntryPoint> entry_point,
                   std::optional<AssistantExitPoint> exit_point) {
        EXPECT_EQ(old_visibility, AssistantVisibility::kVisible);
        EXPECT_EQ(new_visibility, AssistantVisibility::kClosing);
        EXPECT_FALSE(entry_point.has_value());
        EXPECT_EQ(exit_point.value(), AssistantExitPoint::kUnspecified);
      });
  EXPECT_CALL(ui_model_observer_mock, OnUiVisibilityChanged)
      .WillOnce([](AssistantVisibility new_visibility,
                   AssistantVisibility old_visibility,
                   std::optional<AssistantEntryPoint> entry_point,
                   std::optional<AssistantExitPoint> exit_point) {
        EXPECT_EQ(old_visibility, AssistantVisibility::kClosing);
        EXPECT_EQ(new_visibility, AssistantVisibility::kClosed);
        EXPECT_FALSE(entry_point.has_value());
        EXPECT_EQ(exit_point.value(), AssistantExitPoint::kUnspecified);
      });

  controller()->OpenUrl(GURL("googleassistant://send-feedback"),
                        /*in_background=*/false, /*from_server=*/true);

  ui_model()->RemoveObserver(&ui_model_observer_mock);
}

TEST_F(AssistantControllerImplTestForStartUp, ColorModeIsUpdated) {
  auto* active_user_pref_service =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  ASSERT_TRUE(active_user_pref_service);

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      active_user_pref_service);
  SetUpActiveUser();
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();
  ASSERT_TRUE(assistant_service()->dark_mode_enabled().has_value());
  EXPECT_EQ(initial_dark_mode_status,
            assistant_service()->dark_mode_enabled().value());

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_TRUE(assistant_service()->dark_mode_enabled().has_value());
  EXPECT_NE(initial_dark_mode_status,
            assistant_service()->dark_mode_enabled().value());
}

}  // namespace ash
