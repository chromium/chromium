// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_impl.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gtest/include/gtest/gtest.h"

using PasswordFormList = std::vector<std::unique_ptr<autofill::PasswordForm>>;
using ::testing::Ne;
using ::testing::StrictMock;

namespace extensions {

namespace {

template <typename T>
base::OnceCallback<void(T)> GetCallbackArgument(T* arg) {
  return base::BindOnce([](T* arg, T value) { *arg = std::move(value); },
                        base::Unretained(arg));
}

template <typename T>
class CallbackTracker {
 public:
  CallbackTracker()
      : callback_(base::BindRepeating(&CallbackTracker::Callback,
                                      base::Unretained(this))) {}

  using TypedCallback = base::RepeatingCallback<void(const T&)>;

  const TypedCallback& callback() const { return callback_; }

  size_t call_count() const { return call_count_; }

 private:
  void Callback(const T& args) {
    EXPECT_FALSE(args.empty());
    ++call_count_;
  }

  size_t call_count_ = 0;

  TypedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackTracker);
};

class PasswordEventObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit PasswordEventObserver(const std::string& event_name);

  ~PasswordEventObserver() override;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs();

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override;

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(PasswordEventObserver);
};

PasswordEventObserver::PasswordEventObserver(const std::string& event_name)
    : event_name_(event_name) {}

PasswordEventObserver::~PasswordEventObserver() = default;

base::Value PasswordEventObserver::PassEventArgs() {
  return std::move(event_args_);
}

void PasswordEventObserver::OnBroadcastEvent(const extensions::Event& event) {
  if (event.event_name != event_name_) {
    return;
  }
  event_args_ = event.event_args->Clone();
}

enum class ReauthResult { PASS, FAIL };

bool FakeOsReauthCall(bool* reauth_called,
                      ReauthResult result,
                      password_manager::ReauthPurpose purpose) {
  *reauth_called = true;
  return result == ReauthResult::PASS;
}

std::unique_ptr<KeyedService> BuildPasswordsPrivateEventRouter(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      PasswordsPrivateEventRouter::Create(context));
}

autofill::PasswordForm CreateSampleForm() {
  autofill::PasswordForm form;
  form.origin = GURL("http://abc1.com");
  form.username_value = base::ASCIIToUTF16("test@gmail.com");
  form.password_value = base::ASCIIToUTF16("test");
  return form;
}

}  // namespace

class PasswordsPrivateDelegateImplTest : public testing::Test {
 public:
  PasswordsPrivateDelegateImplTest();
  ~PasswordsPrivateDelegateImplTest() override;

  // Sets up a testing password store and fills it with |forms|.
  void SetUpPasswordStore(std::vector<autofill::PasswordForm> forms);

  // Sets up a testing EventRouter with a production
  // PasswordsPrivateEventRouter.
  void SetUpRouters();

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  extensions::TestEventRouter* event_router_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateDelegateImplTest);
};

PasswordsPrivateDelegateImplTest::PasswordsPrivateDelegateImplTest() = default;

PasswordsPrivateDelegateImplTest::~PasswordsPrivateDelegateImplTest() = default;

void PasswordsPrivateDelegateImplTest::SetUpPasswordStore(
    std::vector<autofill::PasswordForm> forms) {
  scoped_refptr<password_manager::TestPasswordStore> password_store(
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  &profile_,
                  base::BindRepeating(&password_manager::BuildPasswordStore<
                                      content::BrowserContext,
                                      password_manager::TestPasswordStore>))
              .get()));
  for (const autofill::PasswordForm& form : forms) {
    password_store->AddLogin(form);
  }
  // Spin the loop to allow PasswordStore tasks being processed.
  base::RunLoop().RunUntilIdle();
}

void PasswordsPrivateDelegateImplTest::SetUpRouters() {
  event_router_ = extensions::CreateAndUseTestEventRouter(&profile_);
  // Set the production PasswordsPrivateEventRouter::Create as a testing
  // factory, because at some point during the preceding initialization, a null
  // factory is set, resulting in nul PasswordsPrivateEventRouter.
  PasswordsPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
      &profile_, base::BindRepeating(&BuildPasswordsPrivateEventRouter));
}

TEST_F(PasswordsPrivateDelegateImplTest, GetSavedPasswordsList) {
  CallbackTracker<PasswordsPrivateDelegate::UiEntries> tracker;

  PasswordsPrivateDelegateImpl delegate(&profile_);

  delegate.GetSavedPasswordsList(tracker.callback());
  EXPECT_EQ(0u, tracker.call_count());

  PasswordFormList list;
  list.push_back(std::make_unique<autofill::PasswordForm>());
  delegate.SetPasswordList(list);
  EXPECT_EQ(1u, tracker.call_count());

  delegate.GetSavedPasswordsList(tracker.callback());
  EXPECT_EQ(2u, tracker.call_count());
}

TEST_F(PasswordsPrivateDelegateImplTest, GetPasswordExceptionsList) {
  CallbackTracker<PasswordsPrivateDelegate::ExceptionEntries> tracker;

  PasswordsPrivateDelegateImpl delegate(&profile_);

  delegate.GetPasswordExceptionsList(tracker.callback());
  EXPECT_EQ(0u, tracker.call_count());

  PasswordFormList list;
  list.push_back(std::make_unique<autofill::PasswordForm>());
  delegate.SetPasswordExceptionList(list);
  EXPECT_EQ(1u, tracker.call_count());

  delegate.GetPasswordExceptionsList(tracker.callback());
  EXPECT_EQ(2u, tracker.call_count());
}

TEST_F(PasswordsPrivateDelegateImplTest, ChangeSavedPassword) {
  autofill::PasswordForm sample_form = CreateSampleForm();
  SetUpPasswordStore({sample_form});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  // Double check that the contents of the passwords list matches our
  // expectation.
  bool got_passwords = false;
  delegate.GetSavedPasswordsList(base::BindLambdaForTesting(
      [&](const PasswordsPrivateDelegate::UiEntries& password_list) {
        got_passwords = true;
        ASSERT_EQ(1u, password_list.size());
        EXPECT_EQ(sample_form.username_value,
                  base::UTF8ToUTF16(password_list[0].username));
        EXPECT_EQ(sample_form.password_value.size(),
                  size_t{password_list[0].num_characters_in_password});
      }));
  EXPECT_TRUE(got_passwords);

  int sample_form_id = delegate.GetPasswordIdGeneratorForTesting().GenerateId(
      password_manager::CreateSortKey(sample_form));
  delegate.ChangeSavedPassword(sample_form_id, base::ASCIIToUTF16("new_user"),
                               base::ASCIIToUTF16("new_pass"));

  // Spin the loop to allow PasswordStore tasks posted when changing the
  // password to be completed.
  base::RunLoop().RunUntilIdle();

  // Check that the changing the password got reflected in the passwords list.
  got_passwords = false;
  delegate.GetSavedPasswordsList(base::BindLambdaForTesting(
      [&](const PasswordsPrivateDelegate::UiEntries& password_list) {
        got_passwords = true;
        ASSERT_EQ(1u, password_list.size());
        EXPECT_EQ(base::ASCIIToUTF16("new_user"),
                  base::UTF8ToUTF16(password_list[0].username));
        EXPECT_EQ(base::ASCIIToUTF16("new_pass").size(),
                  size_t{password_list[0].num_characters_in_password});
      }));
  EXPECT_TRUE(got_passwords);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestPassedReauthOnView) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  bool reauth_called = false;
  delegate.SetOsReauthCallForTesting(base::BindRepeating(
      &FakeOsReauthCall, &reauth_called, ReauthResult::PASS));

  base::Optional<base::string16> plaintext_password;
  delegate.RequestShowPassword(0, GetCallbackArgument(&plaintext_password),
                               nullptr);
  EXPECT_TRUE(reauth_called);
  EXPECT_TRUE(plaintext_password.has_value());
  EXPECT_EQ(base::ASCIIToUTF16("test"), *plaintext_password);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestFailedReauthOnView) {
  SetUpPasswordStore({CreateSampleForm()});

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  bool reauth_called = false;
  delegate.SetOsReauthCallForTesting(base::BindRepeating(
      &FakeOsReauthCall, &reauth_called, ReauthResult::FAIL));

  base::Optional<base::string16> plaintext_password;
  delegate.RequestShowPassword(0, GetCallbackArgument(&plaintext_password),
                               nullptr);
  EXPECT_TRUE(reauth_called);
  EXPECT_FALSE(plaintext_password.has_value());
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthOnExport) {
  SetUpPasswordStore({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  bool reauth_called = false;
  delegate.SetOsReauthCallForTesting(base::BindRepeating(
      &FakeOsReauthCall, &reauth_called, ReauthResult::PASS));

  EXPECT_CALL(mock_accepted, Run(std::string())).Times(2);

  delegate.ExportPasswords(mock_accepted.Get(), nullptr);
  EXPECT_TRUE(reauth_called);

  // Export should ignore previous reauthentication results.
  reauth_called = false;
  delegate.ExportPasswords(mock_accepted.Get(), nullptr);
  EXPECT_TRUE(reauth_called);
}

TEST_F(PasswordsPrivateDelegateImplTest, TestReauthFailedOnExport) {
  SetUpPasswordStore({CreateSampleForm()});
  StrictMock<base::MockCallback<base::OnceCallback<void(const std::string&)>>>
      mock_accepted;

  PasswordsPrivateDelegateImpl delegate(&profile_);
  // Spin the loop to allow PasswordStore tasks posted on the creation of
  // |delegate| to be completed.
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_accepted, Run(std::string("reauth-failed")));

  bool reauth_called = false;
  delegate.SetOsReauthCallForTesting(base::BindRepeating(
      &FakeOsReauthCall, &reauth_called, ReauthResult::FAIL));

  delegate.ExportPasswords(mock_accepted.Get(), nullptr);
  EXPECT_TRUE(reauth_called);
}

}  // namespace extensions
