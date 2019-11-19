// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/activity_log_task_runner.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#endif

namespace extensions {

class FullStreamUIPolicyTest : public testing::Test {
 public:
  FullStreamUIPolicyTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    base::CommandLine no_program_command_line(base::CommandLine::NO_PROGRAM);
    profile_.reset(new TestingProfile());
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogTesting);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile_.get()))->CreateExtensionService
            (&no_program_command_line, base::FilePath(), false);

    // Run pending async tasks resulting from profile construction to ensure
    // these are complete before the test begins.
    base::RunLoop().RunUntilIdle();
  }

  ~FullStreamUIPolicyTest() override {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    base::RunLoop().RunUntilIdle();
    profile_.reset(NULL);
    base::RunLoop().RunUntilIdle();
  }

  // A wrapper function for CheckReadFilteredData, so that we don't need to
  // enter empty string values for parameters we don't care about.
  void CheckReadData(
      ActivityLogDatabasePolicy* policy,
      const std::string& extension_id,
      int day,
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker) {
    CheckReadFilteredData(policy, extension_id, Action::ACTION_ANY, "", "", "",
                          day, std::move(checker));
  }

  // A helper function to call ReadFilteredData on a policy object and wait for
  // the results to be processed.
  void CheckReadFilteredData(
      ActivityLogDatabasePolicy* policy,
      const std::string& extension_id,
      const Action::ActionType type,
      const std::string& api_name,
      const std::string& page_url,
      const std::string& arg_url,
      const int days_ago,
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker) {
    // Submit a request to the policy to read back some data, and call the
    // checker function when results are available.  This will happen on the
    // database thread.
    policy->ReadFilteredData(
        extension_id, type, api_name, page_url, arg_url, days_ago,
        base::BindOnce(&FullStreamUIPolicyTest::CheckWrapper,
                       std::move(checker),
                       base::RunLoop::QuitCurrentWhenIdleClosureDeprecated()));

    // Set up a timeout for receiving results; if we haven't received anything
    // when the timeout triggers then assume that the test is broken.
    base::CancelableOnceClosure timeout(
        base::BindOnce(&FullStreamUIPolicyTest::TimeoutCallback));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, timeout.callback(), TestTimeouts::action_timeout());

    // Wait for results; either the checker or the timeout callbacks should
    // cause the main loop to exit.
    base::RunLoop().Run();

    timeout.Cancel();
  }

  static void CheckWrapper(
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker,
      base::OnceClosure done,
      std::unique_ptr<Action::ActionVector> results) {
    std::move(checker).Run(std::move(results));
    std::move(done).Run();
  }

  static void TimeoutCallback() {
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
    FAIL() << "Policy test timed out waiting for results";
  }

  static void RetrieveActions_LogAndFetchActions(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions0(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(0, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions1(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(1, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions2(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(2, static_cast<int>(i->size()));
  }

  static void RetrieveActions_FetchFilteredActions300(
      std::unique_ptr<std::vector<scoped_refptr<Action>>> i) {
    ASSERT_EQ(300, static_cast<int>(i->size()));
  }

  static void Arguments_Present(std::unique_ptr<Action::ActionVector> i) {
    scoped_refptr<Action> last = i->front();
    CheckAction(*last, "odlameecjipmbmbejkplpemijjgpljce",
                Action::ACTION_API_CALL, "extension.connect",
                "[\"hello\",\"world\"]", "", "", "");
  }

  static void Arguments_GetTodaysActions(
      std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "Page Title",
                "http://www.arg-url.com/");
    CheckAction(*actions->at(1), "punky", Action::ACTION_API_CALL, "brewster",
                "[\"woof\"]", "", "Page Title", "http://www.arg-url.com/");
  }

  static void Arguments_GetOlderActions(
      std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "", "");
    CheckAction(*actions->at(1), "punky", Action::ACTION_API_CALL, "brewster",
                "[\"woof\"]", "", "", "");
  }

  static void AllURLsRemoved(std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_API_CALL, "lets",
                "[\"vamoose\"]", "", "", "");
    CheckAction(*actions->at(1), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "");
  }

  static void SomeURLsRemoved(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(5, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "Google",
                "http://www.args-url.com/");
    CheckAction(*actions->at(1), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "http://www.google.com/", "Google", "");
    CheckAction(*actions->at(2), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "");
    CheckAction(*actions->at(3), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "http://www.google.com/");
    CheckAction(*actions->at(4), "punky", Action::ACTION_DOM_ACCESS, "lets",
                "[\"vamoose\"]", "", "", "");
  }

  static void CheckAction(const Action& action,
                          const std::string& expected_id,
                          const Action::ActionType& expected_type,
                          const std::string& expected_api_name,
                          const std::string& expected_args_str,
                          const std::string& expected_page_url,
                          const std::string& expected_page_title,
                          const std::string& expected_arg_url) {
    ASSERT_EQ(expected_id, action.extension_id());
    ASSERT_EQ(expected_type, action.action_type());
    ASSERT_EQ(expected_api_name, action.api_name());
    ASSERT_EQ(expected_args_str,
              ActivityLogPolicy::Util::Serialize(action.args()));
    ASSERT_EQ(expected_page_url, action.SerializePageUrl());
    ASSERT_EQ(expected_page_title, action.page_title());
    ASSERT_EQ(expected_arg_url, action.SerializeArgUrl());
    ASSERT_NE(-1, action.action_id());
  }

  // A helper function initializes the policy with a number of actions, calls
  // RemoveActions on a policy object and then checks the result of the
  // deletion.
  void CheckRemoveActions(
      ActivityLogDatabasePolicy* policy,
      const std::vector<int64_t>& action_ids,
      base::OnceCallback<void(std::unique_ptr<Action::ActionVector>)> checker) {
    // Use a mock clock to ensure that events are not recorded on the wrong day
    // when the test is run close to local midnight.
    base::SimpleTestClock mock_clock;
    mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                      base::TimeDelta::FromHours(12));
    policy->SetClockForTesting(&mock_clock);

    // Record some actions
    scoped_refptr<Action> action = new Action(
        "punky1", mock_clock.Now() - base::TimeDelta::FromMinutes(40),
        Action::ACTION_DOM_ACCESS, "lets1");
    action->mutable_args()->AppendString("vamoose1");
    action->set_page_url(GURL("http://www.google1.com"));
    action->set_page_title("Google1");
    action->set_arg_url(GURL("http://www.args-url1.com"));
    policy->ProcessAction(action);
    // Record the same action twice, so there are multiple entries in the
    // database.
    policy->ProcessAction(action);

    action = new Action("punky2",
                        mock_clock.Now() - base::TimeDelta::FromMinutes(30),
                        Action::ACTION_API_CALL, "lets2");
    action->mutable_args()->AppendString("vamoose2");
    action->set_page_url(GURL("http://www.google2.com"));
    action->set_page_title("Google2");
    action->set_arg_url(GURL("http://www.args-url2.com"));
    policy->ProcessAction(action);
    // Record the same action twice, so there are multiple entries in the
    // database.
    policy->ProcessAction(action);

    // Submit a request to delete actions.
    policy->RemoveActions(action_ids);

    // Check the result of the deletion. The checker function gets all
    // activities in the database.
    CheckReadData(policy, "", -1, std::move(checker));

    // Clean database.
    policy->DeleteDatabase();
    policy->SetClockForTesting(nullptr);
  }

  static void AllActionsDeleted(std::unique_ptr<Action::ActionVector> actions) {
    ASSERT_EQ(0, static_cast<int>(actions->size()));
  }

  static void NoActionsDeleted(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(4, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky2", Action::ACTION_API_CALL, "lets2",
                "[\"vamoose2\"]", "http://www.google2.com/", "Google2",
                "http://www.args-url2.com/");
    ASSERT_EQ(3, actions->at(0)->action_id());
    CheckAction(*actions->at(1), "punky2", Action::ACTION_API_CALL, "lets2",
                "[\"vamoose2\"]", "http://www.google2.com/", "Google2",
                "http://www.args-url2.com/");
    ASSERT_EQ(4, actions->at(1)->action_id());
    CheckAction(*actions->at(2), "punky1", Action::ACTION_DOM_ACCESS, "lets1",
                "[\"vamoose1\"]", "http://www.google1.com/", "Google1",
                "http://www.args-url1.com/");
    ASSERT_EQ(1, actions->at(2)->action_id());
    CheckAction(*actions->at(3), "punky1", Action::ACTION_DOM_ACCESS, "lets1",
                "[\"vamoose1\"]", "http://www.google1.com/", "Google1",
                "http://www.args-url1.com/");
    ASSERT_EQ(2, actions->at(3)->action_id());
  }

  static void Action1Deleted(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky2", Action::ACTION_API_CALL, "lets2",
                "[\"vamoose2\"]", "http://www.google2.com/", "Google2",
                "http://www.args-url2.com/");
    ASSERT_EQ(3, actions->at(0)->action_id());
    CheckAction(*actions->at(1), "punky2", Action::ACTION_API_CALL, "lets2",
                "[\"vamoose2\"]", "http://www.google2.com/", "Google2",
                "http://www.args-url2.com/");
    ASSERT_EQ(4, actions->at(1)->action_id());
  }

  static void Action2Deleted(std::unique_ptr<Action::ActionVector> actions) {
    // These will be in the vector in reverse time order.
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    CheckAction(*actions->at(0), "punky1", Action::ACTION_DOM_ACCESS, "lets1",
                "[\"vamoose1\"]", "http://www.google1.com/", "Google1",
                "http://www.args-url1.com/");
    ASSERT_EQ(1, actions->at(0)->action_id());
    CheckAction(*actions->at(1), "punky1", Action::ACTION_DOM_ACCESS, "lets1",
                "[\"vamoose1\"]", "http://www.google1.com/", "Google1",
                "http://www.args-url1.com/");
    ASSERT_EQ(2, actions->at(1)->action_id());
  }

 protected:
  ExtensionService* extension_service_;
  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

#if defined OS_CHROMEOS
  chromeos::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  std::unique_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(FullStreamUIPolicyTest, Construct) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  action->set_args(std::move(args));
  policy->ProcessAction(action);
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, LogAndFetchActions) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());
  GURL gurl("http://www.google.com");

  // Write some API calls
  scoped_refptr<Action> action_api = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_API_CALL,
                                                "tabs.testMethod");
  action_api->set_args(std::make_unique<base::ListValue>());
  policy->ProcessAction(action_api);

  scoped_refptr<Action> action_dom = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_DOM_ACCESS,
                                                "document.write");
  action_dom->set_args(std::make_unique<base::ListValue>());
  action_dom->set_page_url(gurl);
  policy->ProcessAction(action_dom);

  CheckReadData(
      policy, extension->id(), 0,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_LogAndFetchActions));

  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, LogAndFetchFilteredActions) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());
  GURL gurl("http://www.google.com");

  // Write some API calls
  scoped_refptr<Action> action_api = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_API_CALL,
                                                "tabs.testMethod");
  action_api->set_args(std::make_unique<base::ListValue>());
  policy->ProcessAction(action_api);

  scoped_refptr<Action> action_dom = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_DOM_ACCESS,
                                                "document.write");
  action_dom->set_args(std::make_unique<base::ListValue>());
  action_dom->set_page_url(gurl);
  policy->ProcessAction(action_dom);

  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_API_CALL, "tabs.testMethod", "",
      "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  // Test for case insensitive matching for api_call.
  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_API_CALL, "tabs.testmethod", "",
      "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  // Test for prefix matching for api_call.
  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_API_CALL, "tabs.testM", "", "",
      -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "", "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "http://www.google.com/", "",
      -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "http://www.google.com", "",
      -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, "", Action::ACTION_DOM_ACCESS, "", "http://www.goo", "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));

  CheckReadFilteredData(
      policy, extension->id(), Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions2));

  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, LogWithArguments) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, std::make_unique<base::Value>("hello"));
  args->Set(1, std::make_unique<base::Value>("world"));
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "extension.connect");
  action->set_args(std::move(args));

  policy->ProcessAction(action);
  CheckReadData(policy, extension->id(), 0,
                base::BindOnce(&FullStreamUIPolicyTest::Arguments_Present));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, GetTodaysActions) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.  Note: Ownership is passed
  // to the policy, but we still keep a pointer locally.  The policy will take
  // care of destruction; this is safe since the policy outlives all our
  // accesses to the mock clock.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(&mock_clock);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky", mock_clock.Now() - base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL, "brewster");
  action->mutable_args()->AppendString("woof");
  action->set_arg_url(GURL("http://www.arg-url.com"));
  action->set_page_title("Page Title");
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_arg_url(GURL("http://www.arg-url.com"));
  action->set_page_title("Page Title");
  policy->ProcessAction(action);

  action = new Action("scoobydoo", mock_clock.Now(), Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_arg_url(GURL("http://www.arg-url.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 0,
      base::BindOnce(&FullStreamUIPolicyTest::Arguments_GetTodaysActions));
  policy->Close();
}

// Check that we can read back less recent actions in the db.
TEST_F(FullStreamUIPolicyTest, GetOlderActions) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(&mock_clock);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock.Now() - base::TimeDelta::FromDays(3) -
                     base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL, "brewster");
  action->mutable_args()->AppendString("woof");
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock.Now() - base::TimeDelta::FromDays(3),
                      Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("too new");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("punky", mock_clock.Now() - base::TimeDelta::FromDays(7),
                      Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("too old");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy, "punky", 3,
      base::BindOnce(&FullStreamUIPolicyTest::Arguments_GetOlderActions));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, RemoveAllURLs) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(&mock_clock);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  mock_clock.Advance(base::TimeDelta::FromSeconds(1));
  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_API_CALL, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google2.com"));
  action->set_page_title("Google");
  // Deliberately no arg url set to make sure it still works when there is no
  // arg url.
  policy->ProcessAction(action);

  // Clean all the URLs.
  std::vector<GURL> no_url_restrictions;
  policy->RemoveURLs(no_url_restrictions);

  CheckReadData(policy, "punky", 0,
                base::BindOnce(&FullStreamUIPolicyTest::AllURLsRemoved));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, RemoveSpecificURLs) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(&mock_clock);

  // Record some actions
  // This should have the page url and args url cleared.
  scoped_refptr<Action> action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google1.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google1.com"));
  policy->ProcessAction(action);

  // This should have the page url cleared but not args url.
  mock_clock.Advance(base::TimeDelta::FromSeconds(1));
  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google1.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  // This should have the page url cleared. The args url is deliberately not set
  // to make sure this doesn't cause any issues.
  mock_clock.Advance(base::TimeDelta::FromSeconds(1));
  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google2.com"));
  action->set_page_title("Google");
  policy->ProcessAction(action);

  // This should have the args url cleared but not the page url or page title.
  mock_clock.Advance(base::TimeDelta::FromSeconds(1));
  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google1.com"));
  policy->ProcessAction(action);

  // This should have neither cleared.
  mock_clock.Advance(base::TimeDelta::FromSeconds(1));
  action =
      new Action("punky", mock_clock.Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.args-url.com"));
  policy->ProcessAction(action);

  // Clean some URLs.
  std::vector<GURL> urls;
  urls.push_back(GURL("http://www.google1.com"));
  urls.push_back(GURL("http://www.google2.com"));
  urls.push_back(GURL("http://www.url_not_in_db.com"));
  policy->RemoveURLs(urls);

  CheckReadData(policy, "punky", 0,
                base::BindOnce(&FullStreamUIPolicyTest::SomeURLsRemoved));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, RemoveExtensionData) {
  FullStreamUIPolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock mock_clock;
  mock_clock.SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(&mock_clock);

  // Record some actions
  scoped_refptr<Action> action =
      new Action("deleteextensiondata", mock_clock.Now(),
                 Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);
  policy->ProcessAction(action);
  policy->ProcessAction(action);

  scoped_refptr<Action> action2 = new Action("dontdelete", mock_clock.Now(),
                                             Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_title("Google");
  action->set_arg_url(GURL("http://www.google.com"));
  policy->ProcessAction(action2);

  policy->Flush();
  policy->RemoveExtensionData("deleteextensiondata");

  CheckReadFilteredData(
      policy, "deleteextensiondata", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions0));

  CheckReadFilteredData(
      policy, "dontdelete", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions1));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, CapReturns) {
  FullStreamUIPolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();

  for (int i = 0; i < 305; i++) {
    scoped_refptr<Action> action =
        new Action("punky",
                   base::Time::Now(),
                   Action::ACTION_API_CALL,
                   base::StringPrintf("apicall_%d", i));
    policy->ProcessAction(action);
  }

  policy->Flush();
  GetActivityLogTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
  base::RunLoop().Run();

  CheckReadFilteredData(
      policy, "punky", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions300));
  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, DeleteDatabase) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "Test extension")
                           .Set("version", "1.0.0")
                           .Set("manifest_version", 2)
                           .Build())
          .Build();
  extension_service_->AddExtension(extension.get());
  GURL gurl("http://www.google.com");

  // Write some API calls.
  scoped_refptr<Action> action_api = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_API_CALL,
                                                "tabs.testMethod");
  action_api->set_args(std::make_unique<base::ListValue>());
  policy->ProcessAction(action_api);

  scoped_refptr<Action> action_dom = new Action(extension->id(),
                                                base::Time::Now(),
                                                Action::ACTION_DOM_ACCESS,
                                                "document.write");
  action_dom->set_args(std::make_unique<base::ListValue>());
  action_dom->set_page_url(gurl);
  policy->ProcessAction(action_dom);

  CheckReadData(
      policy, extension->id(), 0,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_LogAndFetchActions));

  // Now delete them.
  policy->DeleteDatabase();

  CheckReadFilteredData(
      policy, "", Action::ACTION_ANY, "", "", "", -1,
      base::BindOnce(
          &FullStreamUIPolicyTest::RetrieveActions_FetchFilteredActions0));

  policy->Close();
}

TEST_F(FullStreamUIPolicyTest, RemoveActions) {
  ActivityLogDatabasePolicy* policy = new FullStreamUIPolicy(profile_.get());
  policy->Init();

  std::vector<int64_t> action_ids;

  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&FullStreamUIPolicyTest::NoActionsDeleted));

  action_ids.push_back(-1);
  action_ids.push_back(-10);
  action_ids.push_back(0);
  action_ids.push_back(5);
  action_ids.push_back(10);
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&FullStreamUIPolicyTest::NoActionsDeleted));
  action_ids.clear();

  for (int i = 0; i < 50; i++) {
    action_ids.push_back(i + 5);
  }
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&FullStreamUIPolicyTest::NoActionsDeleted));
  action_ids.clear();

  // CheckRemoveActions pushes four actions to the Activity Log database with
  // IDs 1, 2, 3, and 4.
  action_ids.push_back(1);
  action_ids.push_back(2);
  action_ids.push_back(3);
  action_ids.push_back(4);
  CheckRemoveActions(
      policy, action_ids,
      base::BindOnce(&FullStreamUIPolicyTest::AllActionsDeleted));
  action_ids.clear();

  action_ids.push_back(1);
  action_ids.push_back(2);
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&FullStreamUIPolicyTest::Action1Deleted));
  action_ids.clear();

  action_ids.push_back(3);
  action_ids.push_back(4);
  CheckRemoveActions(policy, action_ids,
                     base::BindOnce(&FullStreamUIPolicyTest::Action2Deleted));
  action_ids.clear();

  policy->Close();
}

}  // namespace extensions
