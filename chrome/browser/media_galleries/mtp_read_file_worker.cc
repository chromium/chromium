// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/mtp_read_file_worker.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/media_galleries/snapshot_file_details.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using storage_monitor::StorageMonitor;

namespace {

// Appends |data| to the snapshot file specified by the |snapshot_file_path| on
// a background sequence that allows IO. Returns the number of bytes written to
// the snapshot file. In case of failure, returns zero.
uint32_t WriteDataChunkIntoSnapshotFileOnFileThread(
    const base::FilePath& snapshot_file_path,
    const std::string& data) {
  return base::AppendToFile(snapshot_file_path, data)
             ? base::checked_cast<uint32_t>(data.size())
             : 0;
}

}  // namespace

MTPReadFileWorker::MTPReadFileWorker(const std::string& device_handle)
    : device_handle_(device_handle) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!device_handle_.empty(), base::NotFatalUntil::M161);
}

MTPReadFileWorker::~MTPReadFileWorker() = default;

void MTPReadFileWorker::WriteDataIntoSnapshotFile(
    SnapshotRequestInfo request_info,
    const base::File::Info& snapshot_file_info) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  ReadDataChunkFromDeviceFile(std::make_unique<SnapshotFileDetails>(
      std::move(request_info), snapshot_file_info));
}

void MTPReadFileWorker::ReadDataChunkFromDeviceFile(
    std::unique_ptr<SnapshotFileDetails> snapshot_file_details) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(snapshot_file_details.get(), base::NotFatalUntil::M161);

  // To avoid calling |snapshot_file_details| methods and passing ownership of
  // |snapshot_file_details| in the same_line.
  SnapshotFileDetails* snapshot_file_details_ptr = snapshot_file_details.get();

  auto* mtp_device_manager =
      StorageMonitor::GetInstance()->media_transfer_protocol_manager();
  mtp_device_manager->ReadFileChunk(
      device_handle_, snapshot_file_details_ptr->file_id(),
      snapshot_file_details_ptr->bytes_written(),
      snapshot_file_details_ptr->BytesToRead(),
      base::BindOnce(&MTPReadFileWorker::OnDidReadDataChunkFromDeviceFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(snapshot_file_details)));
}

void MTPReadFileWorker::OnDidReadDataChunkFromDeviceFile(
    std::unique_ptr<SnapshotFileDetails> snapshot_file_details,
    const std::string& data,
    bool error) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(snapshot_file_details.get(), base::NotFatalUntil::M161);
  snapshot_file_details->set_error_occurred(
      error || (data.size() != snapshot_file_details->BytesToRead()));
  if (snapshot_file_details->error_occurred()) {
    OnDidWriteIntoSnapshotFile(std::move(snapshot_file_details));
    return;
  }

  // To avoid calling |snapshot_file_details| methods and passing ownership of
  // |snapshot_file_details| in the same_line.
  SnapshotFileDetails* snapshot_file_details_ptr = snapshot_file_details.get();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&WriteDataChunkIntoSnapshotFileOnFileThread,
                     snapshot_file_details_ptr->snapshot_file_path(), data),
      base::BindOnce(&MTPReadFileWorker::OnDidWriteDataChunkIntoSnapshotFile,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(snapshot_file_details)));
}

void MTPReadFileWorker::OnDidWriteDataChunkIntoSnapshotFile(
    std::unique_ptr<SnapshotFileDetails> snapshot_file_details,
    uint32_t bytes_written) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(snapshot_file_details.get(), base::NotFatalUntil::M161);
  if (snapshot_file_details->AddBytesWritten(bytes_written)) {
    if (!snapshot_file_details->IsSnapshotFileWriteComplete()) {
      ReadDataChunkFromDeviceFile(std::move(snapshot_file_details));
      return;
    }
  } else {
    snapshot_file_details->set_error_occurred(true);
  }
  OnDidWriteIntoSnapshotFile(std::move(snapshot_file_details));
}

void MTPReadFileWorker::OnDidWriteIntoSnapshotFile(
    std::unique_ptr<SnapshotFileDetails> snapshot_file_details) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(snapshot_file_details.get(), base::NotFatalUntil::M161);

  if (snapshot_file_details->error_occurred()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(snapshot_file_details->error_callback(),
                                  base::File::FILE_ERROR_FAILED));
    return;
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(snapshot_file_details->success_callback(),
                                snapshot_file_details->file_info(),
                                snapshot_file_details->snapshot_file_path()));
}
