// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/permission_request_creator_mock.h"

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"

namespace {

std::unique_ptr<base::DictionaryValue> GetManualBehaviorHostDict(
    Profile* profile) {
  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
  const base::DictionaryValue* local_settings =
      settings_service->LocalSettingsForTest();
  std::unique_ptr<base::DictionaryValue> dict_to_insert;

  if (local_settings->HasKey(
          supervised_users::kContentPackManualBehaviorHosts)) {
    const base::DictionaryValue* dict_value;

    local_settings->GetDictionary(
        supervised_users::kContentPackManualBehaviorHosts, &dict_value);

    std::unique_ptr<base::Value> clone =
        std::make_unique<base::Value>(dict_value->Clone());

    dict_to_insert = base::DictionaryValue::From(std::move(clone));
  } else {
    dict_to_insert = std::make_unique<base::DictionaryValue>();
  }

  return dict_to_insert;
}

}  // namespace

PermissionRequestCreatorMock::PermissionRequestCreatorMock(Profile* profile)
    : profile_(profile) {}

PermissionRequestCreatorMock::~PermissionRequestCreatorMock() = default;

bool PermissionRequestCreatorMock::IsEnabled() const {
  return enabled_;
}

void PermissionRequestCreatorMock::CreateURLAccessRequest(
    const GURL& url_requested,
    SuccessCallback callback) {
  DCHECK(enabled_);
  url_requests_.push_back(url_requested);
  if (!delay_handling_)
    CreateURLAccessRequestImpl(url_requested);
  std::move(callback).Run(true);
}

void PermissionRequestCreatorMock::SetPermissionResult(bool result) {
  result_ = result;
}

void PermissionRequestCreatorMock::SetEnabled() {
  enabled_ = true;
}

void PermissionRequestCreatorMock::DelayHandlingForNextRequests() {
  DCHECK(!delay_handling_);

  last_url_request_handled_index_ = url_requests_.size() - 1;
  delay_handling_ = true;
}

void PermissionRequestCreatorMock::HandleDelayedRequests() {
  DCHECK(delay_handling_);

  auto dict_to_insert = GetManualBehaviorHostDict(profile_);

  for (size_t i = last_url_request_handled_index_ + 1; i < url_requests_.size();
       i++) {
    dict_to_insert->SetKey(url_requests_[i].host(), base::Value(result_));
  }

  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          profile_->GetProfileKey());

  settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts,
      std::move(dict_to_insert));

  delay_handling_ = false;
}

void PermissionRequestCreatorMock::CreateURLAccessRequestImpl(
    const GURL& url_requested) {
  auto dict_to_insert = GetManualBehaviorHostDict(profile_);
  dict_to_insert->SetKey(url_requested.host(), base::Value(result_));

  SupervisedUserSettingsService* settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(
          profile_->GetProfileKey());
  settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts,
      std::move(dict_to_insert));
}
