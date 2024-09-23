// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/storage/simple_size_calculator.h"

#include <cstdint>
#include <numeric>

#include "base/memory/scoped_refptr.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

void GetTotalDiskSpaceBlocking(const base::FilePath& mount_path,
                               int64_t* total_bytes) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  if (size >= 0) {
    *total_bytes = size;
  }
}

void GetFreeDiskSpaceBlocking(const base::FilePath& mount_path,
                              int64_t* available_bytes) {
  int64_t size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  if (size >= 0) {
    *available_bytes = size;
  }
}

}  // namespace

SimpleSizeCalculator::SimpleSizeCalculator(
    const CalculationType& calculation_type)
    : calculation_type_(calculation_type) {}

SimpleSizeCalculator::~SimpleSizeCalculator() {}

void SimpleSizeCalculator::StartCalculation() {
  if (calculating_) {
    return;
  }
  calculating_ = true;
  PerformCalculation();
}

void SimpleSizeCalculator::AddObserver(
    SimpleSizeCalculator::Observer* observer) {
  observers_.AddObserver(observer);
}

void SimpleSizeCalculator::RemoveObserver(
    SimpleSizeCalculator::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SimpleSizeCalculator::NotifySizeCalculated(int64_t total_bytes) {
  calculating_ = false;
  for (SimpleSizeCalculator::Observer& observer : observers_) {
    observer.OnSizeCalculated(calculation_type_, total_bytes);
  }
}

TotalDiskSpaceCalculator::TotalDiskSpaceCalculator(Profile* profile)
    : SimpleSizeCalculator(CalculationType::kTotal), profile_(profile) {}

TotalDiskSpaceCalculator::~TotalDiskSpaceCalculator() = default;

void TotalDiskSpaceCalculator::PerformCalculation() {
  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    GetTotalDiskSpace();
    return;
  }
  GetRootDeviceSize();
}

void TotalDiskSpaceCalculator::GetRootDeviceSize() {
  SpacedClient::Get()->GetRootDeviceSize(
      base::BindOnce(&TotalDiskSpaceCalculator::OnGetRootDeviceSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TotalDiskSpaceCalculator::OnGetRootDeviceSize(
    std::optional<int64_t> reply) {
  if (reply.has_value()) {
    if (reply.value() < 0) {
      LOG(DFATAL) << "Negative root device size (" << reply.value() << ")";
    }
    NotifySizeCalculated(reply.value());
    return;
  }

  // FakeSpacedClient does not have a proper implementation of
  // GetRootDeviceSize. If SpacedClient::GetRootDeviceSize does not return a
  // value, use GetTotalDiskSpace as a fallback.
  VLOG(1) << "SpacedClient::OnGetRootDeviceSize: Empty reply. Using "
             "GetTotalDiskSpace as fallback.";
  GetTotalDiskSpace();
}

void TotalDiskSpaceCalculator::GetTotalDiskSpace() {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);

  int64_t* total_bytes = new int64_t(-1);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetTotalDiskSpaceBlocking, my_files_path, total_bytes),
      base::BindOnce(&TotalDiskSpaceCalculator::OnGetTotalDiskSpace,
                     weak_ptr_factory_.GetWeakPtr(), base::Owned(total_bytes)));
}

void TotalDiskSpaceCalculator::OnGetTotalDiskSpace(int64_t* total_bytes) {
  if (*total_bytes < 0) {
    LOG(DFATAL) << "Negative total disk space (" << *total_bytes << ")";
  }
  NotifySizeCalculated(*total_bytes);
}

FreeDiskSpaceCalculator::FreeDiskSpaceCalculator(Profile* profile)
    : SimpleSizeCalculator(CalculationType::kAvailable), profile_(profile) {}

FreeDiskSpaceCalculator::~FreeDiskSpaceCalculator() = default;

void FreeDiskSpaceCalculator::PerformCalculation() {
  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    GetFreeDiskSpace();
    return;
  }
  GetUserFreeDiskSpace();
}

void FreeDiskSpaceCalculator::GetUserFreeDiskSpace() {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  SpacedClient::Get()->GetFreeDiskSpace(
      my_files_path.value(),
      base::BindOnce(&FreeDiskSpaceCalculator::OnGetUserFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreeDiskSpaceCalculator::OnGetUserFreeDiskSpace(
    std::optional<int64_t> reply) {
  if (reply.has_value()) {
    if (reply.value() < 0) {
      LOG(DFATAL) << "Negative user free disk space (" << reply.value() << ")";
    }
    NotifySizeCalculated(reply.value());
    return;
  }

  // FakeSpacedClient does not have a proper implementation of
  // GetFreeDiskSpace. If SpacedClient::GetFreeDiskSpace does not return a
  // value, use GetFreeDiskSpaceBlocking as a fallback.
  VLOG(1) << "SpacedClient::GetFreeDiskSpace: Empty reply. Using "
             "GetFreeDiskSpaceBlocking as fallback.";
  GetFreeDiskSpace();
}

void FreeDiskSpaceCalculator::GetFreeDiskSpace() {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);

  int64_t* available_bytes = new int64_t(-1);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetFreeDiskSpaceBlocking, my_files_path, available_bytes),
      base::BindOnce(&FreeDiskSpaceCalculator::OnGetFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(available_bytes)));
}

void FreeDiskSpaceCalculator::OnGetFreeDiskSpace(int64_t* available_bytes) {
  if (*available_bytes < 0) {
    LOG(DFATAL) << "Negative free disk space (" << *available_bytes << ")";
  }
  NotifySizeCalculated(*available_bytes);
}

}  // namespace ash
