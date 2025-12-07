// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_LAUNCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"

// EnrollmentLauncher is typically lazy created and initialized which makes it
// hard to inject mocks directly and set expectations on them.
// Instead, test that want mock enrollment behavior can set their expectations
// on `MockEnrollmentLauncher` and inject `FakeEnrollmentLauncher` which passes
// calls to the mock:
//
// TEST_F(My, Test) {
//   // Test owns mock and can set expectations on it.
//   StrictMock<MockEnrollmentLauncher> mock;
//   // Override `EnrollmentLauncher` factory which will produce
//   // `FakeEnrollmentLauncher` passing calls to our mock.
//   ScopedEnrollmentLauncherFactoryOverrideForTesting factory(
//       base::BindRepeating(FakeEnrollmentLauncher::Create, &mock));
//
//   EXPECT_CALL(mock, Setup(_, _, _));
//
//   // This is a tested class which will create enrollment launchers during
//   // Foo::Run.
//   Foo foo;
//   foo.Run();
//
//   // `mock` goes out f scope and all set expectations are verified.
//   // The test does not care how and how many fake launchers were created by
//   // `foo`.
// }
namespace ash {

class FakeEnrollmentLauncher;

// Mocks out EnrollmentLauncher.
class MockEnrollmentLauncher {
 public:
  static const char kTestAuthCode[];

  MockEnrollmentLauncher();
  ~MockEnrollmentLauncher();

  MOCK_METHOD(void,
              Setup,
              (const policy::EnrollmentConfig& enrollment_config,
               const std::string& enrolling_user_domain));
  MOCK_METHOD(void, EnrollUsingAuthCode, (const std::string& auth_code));
  MOCK_METHOD(void, EnrollUsingToken, (const std::string& token));
  MOCK_METHOD(void, EnrollUsingAttestation, ());
  MOCK_METHOD(void, EnrollUsingEnrollmentToken, ());
  MOCK_METHOD(void, RestoreAfterRollback, ());
  MOCK_METHOD(void, GetDeviceAttributeUpdatePermission, ());
  MOCK_METHOD(void,
              UpdateDeviceAttributes,
              (const std::string& asset_id, const std::string& location));
  MOCK_METHOD(void,
              ClearAuth,
              (base::OnceClosure callback, bool revoke_oauth2_tokens));
  MOCK_METHOD(bool, InProgress, (), (const));
  MOCK_METHOD(std::string, GetOAuth2RefreshToken, (), (const));

  // Returns status consumer associated with the bound fake launcher. Crashes if
  // no fake launcher was create and bound to the mock.
  EnrollmentLauncher::EnrollmentStatusConsumer* status_consumer();

 private:
  friend class FakeEnrollmentLauncher;

  // Binds a fake launcher to the mock.
  void set_fake_launcher(
      FakeEnrollmentLauncher* fake_launcher,
      EnrollmentLauncher::EnrollmentStatusConsumer* status_consumer);

  // Unbinds the fake launcher from the mock.
  void unset_fake_launcher(FakeEnrollmentLauncher* fake_launcher);

  raw_ptr<FakeEnrollmentLauncher> current_fake_launcher_{nullptr};
  raw_ptr<EnrollmentLauncher::EnrollmentStatusConsumer> status_consumer_{
      nullptr};
};

// Connects `EnrollmentLauncher::EnrollmentStatusConsumer`,
// `FakeEnrollmentLauncher`, owned by status consumer, and
// `MockEnrollmentLauncher`  consumer together. The connection exists from
// the fake launcher's construction till destruction.
class FakeEnrollmentLauncher : public EnrollmentLauncher {
 public:
  FakeEnrollmentLauncher(const FakeEnrollmentLauncher&) = delete;
  FakeEnrollmentLauncher& operator=(const FakeEnrollmentLauncher&) = delete;

  // Factory method to create fake launcher and bind it to `mock`. The `mock` is
  // not owned and must outlive the fake launcher.
  static std::unique_ptr<EnrollmentLauncher> Create(
      MockEnrollmentLauncher* mock,
      EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain);

  ~FakeEnrollmentLauncher() override;

  // EnrollmentLauncher:
  void EnrollUsingAuthCode(const std::string& auth_code) override;
  void EnrollUsingToken(const std::string& token) override;
  void EnrollUsingAttestation() override;
  void EnrollUsingEnrollmentToken() override;
  void ClearAuth(base::OnceClosure callback,
                 bool revoke_oauth2_tokens) override;
  void GetDeviceAttributeUpdatePermission() override;
  void UpdateDeviceAttributes(const std::string& asset_id,
                              const std::string& location) override;
  void Setup(const policy::EnrollmentConfig& enrollment_config,
             const std::string& enrolling_user_domain) override;
  bool InProgress() const override;
  std::string GetOAuth2RefreshToken() const override;

 private:
  FakeEnrollmentLauncher(MockEnrollmentLauncher* mock,
                         EnrollmentStatusConsumer* status_consumer);

  raw_ptr<MockEnrollmentLauncher> mock_{nullptr};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_LAUNCHER_H_
