// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_statistics.h"

#include <utility>

#include "chrome/browser/profiles/profile_statistics_aggregator.h"

ProfileStatistics::ProfileStatistics(
    scoped_refptr<autofill::AutofillWebDataService> autofill_web_data_service,
    autofill::PersonalDataManager* personal_data_manager,
    bookmarks::BookmarkModel* bookmark_model,
    history::HistoryService* history_service,
    scoped_refptr<password_manager::PasswordStoreInterface>
        profile_password_store,
    PrefService* pref_service,
    user_annotations::UserAnnotationsService* user_annotations_service,
    std::unique_ptr<device::fido::PlatformCredentialStore>
        platform_credential_store)
    : autofill_web_data_service_(std::move(autofill_web_data_service)),
      personal_data_manager_(personal_data_manager),
      bookmark_model_(bookmark_model),
      history_service_(history_service),
      profile_password_store_(profile_password_store),
      pref_service_(pref_service),
      user_annotations_service_(user_annotations_service),
      platform_credential_store_(std::move(platform_credential_store)),
      aggregator_(nullptr) {}

ProfileStatistics::~ProfileStatistics() = default;

void ProfileStatistics::GatherStatistics(
    profiles::ProfileStatisticsCallback callback) {
  if (!aggregator_) {
    aggregator_ = std::make_unique<ProfileStatisticsAggregator>(
        autofill_web_data_service_, personal_data_manager_, bookmark_model_,
        history_service_, profile_password_store_, pref_service_,
        user_annotations_service_, std::move(platform_credential_store_),
        base::BindOnce(&ProfileStatistics::DeregisterAggregator,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  aggregator_->AddCallbackAndStartAggregator(std::move(callback));
}

void ProfileStatistics::DeregisterAggregator() {
  aggregator_ = nullptr;
}
