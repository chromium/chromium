// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOGGER_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOGGER_BASE_H_

#include "ash/components/arc/arc_prefs.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/disks/disk.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace policy {

// Contains shared functionality to manage logs of install events
// (of type Event) for apps keyed by AppId.
// The term |app| in this class refers to ARC++ apps as well as extensions.
template <class Event, class EventType, class AppId>
class InstallEventLoggerBase {
 public:
  explicit InstallEventLoggerBase(Profile* profile) : profile_(profile) {
    stateful_path_ = ash::disks::GetStatefulPartitionPath();
  }

  std::unique_ptr<Event> CreateEvent(EventType type) {
    std::unique_ptr<Event> event = std::make_unique<Event>();
    EnsureTimestampSet(event.get());
    event->set_event_type(type);
    return event;
  }

  void EnsureTimestampSet(Event* event) {
    if (!event->has_timestamp()) {
      event->set_timestamp(
          (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
    }
  }

  // Return all elements that are members of |first| but not |second|.
  std::set<AppId> GetDifference(const std::set<AppId>& first,
                                const std::set<AppId>& second) {
    std::set<AppId> difference;
    std::set_difference(first.begin(), first.end(), second.begin(),
                        second.end(),
                        std::inserter(difference, difference.end()));
    return difference;
  }

  void AddForSetOfAppsWithDiskSpaceInfo(const std::set<AppId>& ids,
                                        std::unique_ptr<Event> event) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&AddDiskSpaceInfoToEvent, std::move(event),
                       stateful_path_),
        base::BindOnce(&InstallEventLoggerBase::AddForSetOfApps,
                       weak_factory_.GetWeakPtr(), ids));
  }

  static std::unique_ptr<Event> AddDiskSpaceInfoToEvent(
      std::unique_ptr<Event> event,
      const base::FilePath& stateful_path) {
    const int64_t stateful_total =
        base::SysInfo::AmountOfTotalDiskSpace(stateful_path);
    if (stateful_total >= 0) {
      event->set_stateful_total(stateful_total);
    }
    const int64_t stateful_free =
        base::SysInfo::AmountOfFreeDiskSpace(stateful_path);
    if (stateful_free >= 0) {
      event->set_stateful_free(stateful_free);
    }
    return event;
  }

  // Set stateful partition path for unit tests.
  void SetStatefulPathForTesting(const base::FilePath& path) {
    stateful_path_ = path;
  }

 protected:
  // The profile whose install requests to log.
  // This applies both to ARC++ apps and extensions.
  const raw_ptr<Profile> profile_;

  // Adds |event| to the log for all apps in |ids|.
  virtual void AddForSetOfApps(const std::set<AppId>& ids,
                               std::unique_ptr<Event> event) = 0;

  void AddEvent(const AppId& appId,
                bool gather_disk_space_info,
                std::unique_ptr<Event>& event) {
    EnsureTimestampSet(event.get());
    if (gather_disk_space_info) {
      AddForSetOfAppsWithDiskSpaceInfo({appId}, std::move(event));
    } else {
      AddForSetOfApps({appId}, std::move(event));
    }
  }

 private:
  // Path for stateful partition.
  base::FilePath stateful_path_;

  // Weak factory used to reference |this| from background tasks.
  base::WeakPtrFactory<InstallEventLoggerBase> weak_factory_{this};
};
}  // namespace policy
#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOGGER_BASE_H_
