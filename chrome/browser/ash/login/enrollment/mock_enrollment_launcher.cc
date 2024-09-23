// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"

namespace ash {

MockEnrollmentLauncher::MockEnrollmentLauncher() = default;

MockEnrollmentLauncher::~MockEnrollmentLauncher() {
  CHECK(!current_fake_launcher_)
      << "Mock enrollment launcher must outlive all its fake proxies to ensure "
         "it won't access dangling pointers.";
  CHECK(!status_consumer_);
}

void MockEnrollmentLauncher::set_fake_launcher(
    FakeEnrollmentLauncher* fake_launcher,
    EnrollmentLauncher::EnrollmentStatusConsumer* status_consumer) {
  current_fake_launcher_ = fake_launcher;
  status_consumer_ = status_consumer;
}

void MockEnrollmentLauncher::unset_fake_launcher(
    FakeEnrollmentLauncher* fake_launcher) {
  if (current_fake_launcher_ != fake_launcher) {
    return;
  }

  current_fake_launcher_ = nullptr;
  status_consumer_ = nullptr;
}

EnrollmentLauncher::EnrollmentStatusConsumer*
MockEnrollmentLauncher::status_consumer() {
  CHECK(status_consumer_);
  return status_consumer_;
}

// static
std::unique_ptr<EnrollmentLauncher> FakeEnrollmentLauncher::Create(
    MockEnrollmentLauncher* mock,
    EnrollmentStatusConsumer* status_consumer,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  auto result =
      base::WrapUnique(new FakeEnrollmentLauncher(mock, status_consumer));
  result->Setup(enrollment_config, enrolling_user_domain);
  return result;
}

FakeEnrollmentLauncher::FakeEnrollmentLauncher(
    MockEnrollmentLauncher* mock,
    EnrollmentStatusConsumer* status_consumer)
    : mock_(mock) {
  mock_->set_fake_launcher(this, status_consumer);
}

FakeEnrollmentLauncher::~FakeEnrollmentLauncher() {
  mock_->unset_fake_launcher(this);
}

void FakeEnrollmentLauncher::EnrollUsingAuthCode(const std::string& auth_code) {
  mock_->EnrollUsingAuthCode(auth_code);
}

void FakeEnrollmentLauncher::EnrollUsingToken(const std::string& token) {
  mock_->EnrollUsingToken(token);
}

void FakeEnrollmentLauncher::EnrollUsingAttestation() {
  mock_->EnrollUsingAttestation();
}

void FakeEnrollmentLauncher::EnrollUsingEnrollmentToken() {
  mock_->EnrollUsingEnrollmentToken();
}

void FakeEnrollmentLauncher::ClearAuth(base::OnceClosure callback,
                                       bool revoke_oauth2_tokens) {
  mock_->ClearAuth(std::move(callback), revoke_oauth2_tokens);
}

void FakeEnrollmentLauncher::GetDeviceAttributeUpdatePermission() {
  mock_->GetDeviceAttributeUpdatePermission();
}

void FakeEnrollmentLauncher::UpdateDeviceAttributes(
    const std::string& asset_id,
    const std::string& location) {
  mock_->UpdateDeviceAttributes(asset_id, location);
}

void FakeEnrollmentLauncher::Setup(
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  mock_->Setup(enrollment_config, enrolling_user_domain);
}

bool FakeEnrollmentLauncher::InProgress() const {
  return mock_->InProgress();
}

std::string FakeEnrollmentLauncher::GetOAuth2RefreshToken() const {
  return mock_->GetOAuth2RefreshToken();
}

}  // namespace ash
