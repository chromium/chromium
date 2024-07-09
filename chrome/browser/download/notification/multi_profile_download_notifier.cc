// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/multi_profile_download_notifier.h"

#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/download/public/common/simple_download_manager.h"
#include "content/public/browser/download_manager.h"

bool MultiProfileDownloadNotifier::Client::ShouldObserveProfile(
    Profile* profile) {
  return true;
}

MultiProfileDownloadNotifier::MultiProfileDownloadNotifier(
    MultiProfileDownloadNotifier::Client* client,
    bool wait_for_manager_initialization)
    : client_(client),
      wait_for_manager_initialization_(wait_for_manager_initialization) {
  DCHECK(client_);
}

MultiProfileDownloadNotifier::~MultiProfileDownloadNotifier() = default;

void MultiProfileDownloadNotifier::AddProfile(Profile* profile) {
  // The multi profile download notifier is not needed for irregular profiles
  // that don't support it, like the system profile. In addition it needs some
  // keyed service that might not be available for those profiles.
  if (AreKeyedServicesDisabledForProfileByDefault(profile) ||
      !client_->ShouldObserveProfile(profile)) {
    return;
  }

  content::DownloadManager* manager = profile->GetDownloadManager();
  if (base::Contains(download_notifiers_, manager,
                     &download::AllDownloadItemNotifier::GetManager)) {
    return;
  }

  profile_observer_.AddObservation(profile);
  download_notifiers_.emplace(
      std::make_unique<download::AllDownloadItemNotifier>(manager, this));

  for (Profile* off_the_record : profile->GetAllOffTheRecordProfiles()) {
    // An OTR profile lists itself among its own OTR profiles.
    if (off_the_record != profile)
      OnOffTheRecordProfileCreated(off_the_record);
  }
}

std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>
MultiProfileDownloadNotifier::GetAllDownloads() {
  download::SimpleDownloadManager::DownloadVector downloads;
  for (const auto& download_notifier : download_notifiers_) {
    content::DownloadManager* manager = download_notifier->GetManager();
    if (manager && IsManagerReady(manager)) {
      manager->GetAllDownloads(&downloads);
    }
  }
  return downloads;
}

download::DownloadItem* MultiProfileDownloadNotifier::GetDownloadByGuid(
    const std::string& guid) {
  for (const auto& notifier : download_notifiers_) {
    content::DownloadManager* manager = notifier->GetManager();
    if (!manager)
      continue;

    download::DownloadItem* item = manager->GetDownloadByGuid(guid);
    if (item)
      return item;
  }

  return nullptr;
}

void MultiProfileDownloadNotifier::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  AddProfile(off_the_record);
}

void MultiProfileDownloadNotifier::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observer_.RemoveObservation(profile);
  // This profile's download notifier is destroyed in `OnManagerGoingDown()`.
}

void MultiProfileDownloadNotifier::OnManagerInitialized(
    content::DownloadManager* manager) {
  client_->OnManagerInitialized(manager);
}

void MultiProfileDownloadNotifier::OnManagerGoingDown(
    content::DownloadManager* manager) {
  client_->OnManagerGoingDown(manager);

  auto it = base::ranges::find(download_notifiers_, manager,
                               &download::AllDownloadItemNotifier::GetManager);
  CHECK(it != download_notifiers_.end(), base::NotFatalUntil::M130);
  download_notifiers_.erase(it);
}

void MultiProfileDownloadNotifier::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  DCHECK(manager);
  if (IsManagerReady(manager))
    client_->OnDownloadCreated(manager, item);
}

void MultiProfileDownloadNotifier::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  DCHECK(manager);
  if (IsManagerReady(manager))
    client_->OnDownloadUpdated(manager, item);
}

void MultiProfileDownloadNotifier::OnDownloadDestroyed(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  DCHECK(manager);
  if (IsManagerReady(manager))
    client_->OnDownloadDestroyed(manager, item);
}

bool MultiProfileDownloadNotifier::IsManagerReady(
    content::DownloadManager* manager) {
  return manager->IsManagerInitialized() || !wait_for_manager_initialization_;
}
