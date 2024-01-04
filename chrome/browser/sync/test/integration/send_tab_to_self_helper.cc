// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"

#include <map>
#include <sstream>

#include "base/check_op.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace send_tab_to_self_helper {

SendTabToSelfUrlChecker::SendTabToSelfUrlChecker(
    send_tab_to_self::SendTabToSelfSyncService* service,
    const GURL& url)
    : url_(url), service_(service) {
  DCHECK(service);
  service->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfUrlChecker::~SendTabToSelfUrlChecker() {
  service_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfUrlChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for url '" + url_.spec() + "' to be populated.";

  send_tab_to_self::SendTabToSelfModel* model =
      service_->GetSendTabToSelfModel();
  for (const std::string& guid : model->GetAllGuids()) {
    if (model->GetEntryByGUID(guid)->GetURL() == url_) {
      return true;
    }
  }
  return false;
}

void SendTabToSelfUrlChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfUrlChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfUrlChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfUrlOpenedChecker::SendTabToSelfUrlOpenedChecker(
    send_tab_to_self::SendTabToSelfSyncService* service,
    const GURL& url)
    : url_(url), service_(service) {
  DCHECK(service);
  service->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfUrlOpenedChecker::~SendTabToSelfUrlOpenedChecker() {
  service_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfUrlOpenedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for data for url '" + url_.spec() + "' to be marked opened.";

  send_tab_to_self::SendTabToSelfModel* model =
      service_->GetSendTabToSelfModel();
  for (const std::string& guid : model->GetAllGuids()) {
    const send_tab_to_self::SendTabToSelfEntry* entry =
        model->GetEntryByGUID(guid);
    if (entry->GetURL() == url_ && entry->IsOpened()) {
      return true;
    }
  }
  return false;
}

void SendTabToSelfUrlOpenedChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfUrlOpenedChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfUrlOpenedChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

void SendTabToSelfUrlOpenedChecker::EntriesOpenedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        opened_entries) {
  CheckExitCondition();
}

SendTabToSelfModelEqualityChecker::SendTabToSelfModelEqualityChecker(
    send_tab_to_self::SendTabToSelfSyncService* service0,
    send_tab_to_self::SendTabToSelfSyncService* service1)
    : service0_(service0), service1_(service1) {
  DCHECK(service0);
  DCHECK(service1);
  service0->GetSendTabToSelfModel()->AddObserver(this);
  service1->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfModelEqualityChecker::~SendTabToSelfModelEqualityChecker() {
  service0_->GetSendTabToSelfModel()->RemoveObserver(this);
  service1_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfModelEqualityChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for services to converge";

  const send_tab_to_self::SendTabToSelfModel* model0 =
      service0_->GetSendTabToSelfModel();
  const send_tab_to_self::SendTabToSelfModel* model1 =
      service1_->GetSendTabToSelfModel();

  if (model0->GetAllGuids() != model1->GetAllGuids()) {
    return false;
  }
  for (const std::string& guid : model0->GetAllGuids()) {
    const send_tab_to_self::SendTabToSelfEntry* entry0 =
        model0->GetEntryByGUID(guid);
    const send_tab_to_self::SendTabToSelfEntry* entry1 =
        model1->GetEntryByGUID(guid);

    DCHECK_NE(entry0, nullptr);
    DCHECK_NE(entry1, nullptr);

    if (entry0->GetGUID() != entry1->GetGUID() ||
        entry0->GetURL() != entry1->GetURL() ||
        entry0->GetTitle() != entry1->GetTitle() ||
        entry0->GetSharedTime() != entry1->GetSharedTime() ||
        entry0->GetDeviceName() != entry1->GetDeviceName()) {
      return false;
    }
  }
  return true;
}

void SendTabToSelfModelEqualityChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfModelEqualityChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfModelEqualityChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfActiveChecker::SendTabToSelfActiveChecker(
    send_tab_to_self::SendTabToSelfSyncService* service)
    : service_(service) {
  DCHECK(service);
  service->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfActiveChecker::~SendTabToSelfActiveChecker() {
  service_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfActiveChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for model to be active.";
  return service_->GetSendTabToSelfModel()->IsReady();
}

void SendTabToSelfActiveChecker::SendTabToSelfModelLoaded() {
  CheckExitCondition();
}

void SendTabToSelfActiveChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  CheckExitCondition();
}

void SendTabToSelfActiveChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

SendTabToSelfMultiDeviceActiveChecker::SendTabToSelfMultiDeviceActiveChecker(
    syncer::DeviceInfoTracker* tracker)
    : tracker_(tracker) {
  tracker_->AddObserver(this);
}

SendTabToSelfMultiDeviceActiveChecker::
    ~SendTabToSelfMultiDeviceActiveChecker() {
  tracker_->RemoveObserver(this);
}

bool SendTabToSelfMultiDeviceActiveChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for multiple devices to be active.";
  const std::map<syncer::DeviceInfo::FormFactor, int> device_count_by_type =
      tracker_->CountActiveDevicesByType();
  int total = 0;
  for (const auto& [type, count] : device_count_by_type) {
    total += count;
  }
  return total > 1;
}

void SendTabToSelfMultiDeviceActiveChecker::OnDeviceInfoChange() {
  CheckExitCondition();
}

SendTabToSelfDeviceDisabledChecker::SendTabToSelfDeviceDisabledChecker(
    syncer::DeviceInfoTracker* tracker,
    const std::string& device_guid)
    : tracker_(tracker), device_guid_(device_guid) {
  tracker_->AddObserver(this);
}

SendTabToSelfDeviceDisabledChecker::~SendTabToSelfDeviceDisabledChecker() {
  tracker_->RemoveObserver(this);
}

bool SendTabToSelfDeviceDisabledChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for device to have send_tab_to_self disabled";
  const syncer::DeviceInfo* device_info = tracker_->GetDeviceInfo(device_guid_);
  return device_info && !device_info->send_tab_to_self_receiving_enabled();
}

void SendTabToSelfDeviceDisabledChecker::OnDeviceInfoChange() {
  CheckExitCondition();
}

SendTabToSelfUrlDeletedChecker::SendTabToSelfUrlDeletedChecker(
    send_tab_to_self::SendTabToSelfSyncService* service,
    const GURL& url)
    : url_(url), service_(service) {
  DCHECK(service);
  service->GetSendTabToSelfModel()->AddObserver(this);
}

SendTabToSelfUrlDeletedChecker::~SendTabToSelfUrlDeletedChecker() {
  service_->GetSendTabToSelfModel()->RemoveObserver(this);
}

bool SendTabToSelfUrlDeletedChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for data for url '" + url_.spec() + "' to be deleted.";

  send_tab_to_self::SendTabToSelfModel* model =
      service_->GetSendTabToSelfModel();
  DCHECK(model);
  // Checks each URL in the model and returns passes if URL in question is not
  // found.
  for (const std::string& guid : model->GetAllGuids()) {
    if (model->GetEntryByGUID(guid)->GetURL() == url_) {
      return false;
    }
  }
  return true;
}

void SendTabToSelfUrlDeletedChecker::SendTabToSelfModelLoaded() {
  // This ensures that the URL being inspected is present when the model loads.
  std::ostringstream s;
  DCHECK(!IsExitConditionSatisfied(&s));
}

void SendTabToSelfUrlDeletedChecker::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {}

void SendTabToSelfUrlDeletedChecker::EntriesRemovedRemotely(
    const std::vector<std::string>& guids_removed) {
  CheckExitCondition();
}

}  // namespace send_tab_to_self_helper
