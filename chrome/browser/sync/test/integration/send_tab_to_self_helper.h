// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEND_TAB_TO_SELF_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEND_TAB_TO_SELF_HELPER_H_

#include <string>
#include <vector>

#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "url/gurl.h"

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfSyncService;
}  // namespace send_tab_to_self

namespace send_tab_to_self_helper {

// Class that allows waiting until a particular |url| is exposed by the
// SendTabToSelfModel in |service|.
class SendTabToSelfUrlChecker
    : public StatusChangeChecker,
      public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  // The caller must ensure that |service| is not null and will outlive this
  // object.
  SendTabToSelfUrlChecker(send_tab_to_self::SendTabToSelfSyncService* service,
                          const GURL& url);
  ~SendTabToSelfUrlChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void EntriesAddedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void EntriesRemovedRemotely(
      const std::vector<std::string>& guids_removed) override;

 private:
  const GURL url_;
  send_tab_to_self::SendTabToSelfSyncService* const service_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfUrlChecker);
};

// Class that allows waiting until a particular |url| is marked opened by the
// SendTabToSelfModel in |service|.
class SendTabToSelfUrlOpenedChecker
    : public StatusChangeChecker,
      public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  // The caller must ensure that |service| is not null and will outlive this
  // object.
  SendTabToSelfUrlOpenedChecker(
      send_tab_to_self::SendTabToSelfSyncService* service,
      const GURL& url);
  ~SendTabToSelfUrlOpenedChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void EntriesAddedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void EntriesRemovedRemotely(
      const std::vector<std::string>& guids_removed) override;
  void EntriesOpenedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          opened_entries) override;

 private:
  const GURL url_;
  send_tab_to_self::SendTabToSelfSyncService* const service_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfUrlOpenedChecker);
};

// Class that allows waiting the number of entries in until |service0|
// matches the number of entries in |service1|.
class SendTabToSelfModelEqualityChecker
    : public StatusChangeChecker,
      public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  // The caller must ensure that |service0| and |service1| are not null and
  // will outlive this object.
  SendTabToSelfModelEqualityChecker(
      send_tab_to_self::SendTabToSelfSyncService* service0,
      send_tab_to_self::SendTabToSelfSyncService* service1);
  ~SendTabToSelfModelEqualityChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void EntriesAddedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void EntriesRemovedRemotely(
      const std::vector<std::string>& guids_removed) override;

 private:
  send_tab_to_self::SendTabToSelfSyncService* const service0_;
  send_tab_to_self::SendTabToSelfSyncService* const service1_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModelEqualityChecker);
};

// Class that allows waiting until the bridge is ready.
class SendTabToSelfActiveChecker
    : public StatusChangeChecker,
      public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  // The caller must ensure that |service| is not null and will outlive this
  // object.
  explicit SendTabToSelfActiveChecker(
      send_tab_to_self::SendTabToSelfSyncService* service);
  ~SendTabToSelfActiveChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void EntriesAddedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void EntriesRemovedRemotely(
      const std::vector<std::string>& guids_removed) override;

 private:
  send_tab_to_self::SendTabToSelfSyncService* const service_;
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfActiveChecker);
};

// Class that allows waiting until two devices are ready.
class SendTabToSelfMultiDeviceActiveChecker
    : public StatusChangeChecker,
      public syncer::DeviceInfoTracker::Observer {
 public:
  explicit SendTabToSelfMultiDeviceActiveChecker(
      syncer::DeviceInfoTracker* tracker);
  ~SendTabToSelfMultiDeviceActiveChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // DeviceInfoTracker::Observer implementation.
  void OnDeviceInfoChange() override;

 private:
  syncer::DeviceInfoTracker* const tracker_;
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfMultiDeviceActiveChecker);
};

// Class that allows waiting until device has send_tab_to_self disabled.
class SendTabToSelfDeviceDisabledChecker
    : public StatusChangeChecker,
      public syncer::DeviceInfoTracker::Observer {
 public:
  SendTabToSelfDeviceDisabledChecker(syncer::DeviceInfoTracker* tracker,
                                     const std::string& device_guid);
  ~SendTabToSelfDeviceDisabledChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // DeviceInfoTracker::Observer implementation.
  void OnDeviceInfoChange() override;

 private:
  syncer::DeviceInfoTracker* const tracker_;
  std::string device_guid_;
};

class SendTabToSelfUrlDeletedChecker
    : public StatusChangeChecker,
      public send_tab_to_self::SendTabToSelfModelObserver {
 public:
  // The caller must ensure that |service| is not null and will outlive this
  // object.
  SendTabToSelfUrlDeletedChecker(
      send_tab_to_self::SendTabToSelfSyncService* service,
      const GURL& url);
  ~SendTabToSelfUrlDeletedChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SendTabToSelfModelObserver implementation.
  void SendTabToSelfModelLoaded() override;
  void EntriesAddedRemotely(
      const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
          new_entries) override;
  void EntriesRemovedRemotely(
      const std::vector<std::string>& guids_removed) override;

 private:
  const GURL url_;
  send_tab_to_self::SendTabToSelfSyncService* const service_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfUrlDeletedChecker);
};

}  // namespace send_tab_to_self_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SEND_TAB_TO_SELF_HELPER_H_
