// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_H_

#include <stdint.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "crypto/obsolete/md5.h"
#include "extensions/common/extension_id.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider_android.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom-forward.h"
#include "services/device/public/mojom/usb_manager.mojom-forward.h"
#endif

namespace image_writer_api = extensions::api::image_writer_private;

namespace base {
class FilePath;
}  // namespace base

namespace extensions::image_writer {

inline constexpr int kProgressComplete = 100;

class OperationManager;
class ZstdExtractor;

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
class ImageWriterUtilityClient;
#endif

// Encapsulates an operation being run on behalf of the
// OperationManager.  Construction of the operation does not start
// anything.  The operation's Start method should be called to start it, and
// then the Cancel method will stop it.  The operation will call back to the
// OperationManager periodically or on any significant event.
//
// Each stage of the operation is generally divided into multiple phases with
// Start() being the first phase and Complete() being the last. All phases
// except Complete() run on blocking thread and Complete() runs on the UI
// thread.
//
// TODO(haven): This class is current refcounted because it is owned by the
// OperationManager on the UI thread but needs to do work on blocking threads.
// There is probably a better way to organize this so that it can be represented
// by a WeakPtr, but those are not thread-safe.  Additionally, if destruction is
// done on the UI thread then that causes problems if any of the fields were
// allocated/accessed on the blocking thread.  http://crbug.com/41090268
class Operation : public base::RefCountedThreadSafe<Operation> {
 public:
  using StartWriteCallback = base::OnceCallback<void(bool, const std::string&)>;
  using CancelWriteCallback =
      base::OnceCallback<void(bool, const std::string&)>;

  Operation(base::WeakPtr<OperationManager> manager,
            const ExtensionId& extension_id,
            const std::string& device_path,
            const base::FilePath& download_folder);

  Operation(const Operation&) = delete;
  Operation& operator=(const Operation&) = delete;

  // Starts the operation.
  void Start();

  // Cancel the operation. This must be called to clean up internal state and
  // cause the the operation to actually stop.  It will not be destroyed until
  // all callbacks have completed.
  void Cancel();

  // Aborts the operation, cancelling it and generating an error.
  void Abort();

  // Informational getters.
  int GetProgress();
  image_writer_api::Stage GetStage();

  // Posts `task` to Operation's `task_runner_`.
  void PostTask(base::OnceClosure task);

#if BUILDFLAG(IS_ANDROID)
  // Streams a single Write() or VerifyWrite() pass over an open
  // UsbDeviceConnection on Operation's `task_runner_`. Must not outlive the
  // UsbDeviceConnection that created it, and at most one UsbTransfer may be
  // active on a connection at a time. Destroying the UsbTransfer before
  // completion cancels any in-flight USB command without invoking `callback`.
  class UsbTransfer {
   public:
    using CompletionCallback =
        base::OnceCallback<void(base::expected<void, std::string>)>;

    UsbTransfer() = default;
    virtual ~UsbTransfer() = default;

    // Starts streaming the transfer asynchronously on `task_runner_` and
    // invokes `callback` with `base::ok()` on completion or an
    // `error::k*` constant on failure/cancellation. May be called at most
    // once.
    virtual void Start(CompletionCallback callback) = 0;
  };

  // Owns the USB Mass Storage Bulk-Only Transport connection kept open across
  // Write() and VerifyWrite() for a single Operation on `task_runner_`.
  class UsbDeviceConnection {
   public:
    using CompletionCallback =
        base::OnceCallback<void(base::expected<void, std::string>)>;

    UsbDeviceConnection() = default;
    virtual ~UsbDeviceConnection() = default;

    // Opens the USB device, claims its Mass Storage interface, and probes
    // logical block size and capacity via SCSI TEST UNIT READY and
    // READ CAPACITY (10). If any step fails or the operation is cancelled,
    // releases the interface (if claimed) and closes the device (if opened)
    // before invoking `callback` with an `error::k*` constant.
    virtual void Open(CompletionCallback callback) = 0;

    // Cancels any in-flight command, releases the Mass Storage interface (if
    // claimed), and closes the USB device (if opened) before invoking
    // `callback`.
    virtual void Close(base::OnceClosure callback) = 0;

    // Creates a transfer that writes `image_path` to the open USB device,
    // optionally zeroing the trailing backup GPT blocks when
    // `wipe_backup_gpt` is true, and flushes the device cache via SCSI
    // SYNCHRONIZE CACHE (10). Requires `Open()` to have succeeded.
    virtual std::unique_ptr<UsbTransfer> CreateWriteTransfer(
        base::FilePath image_path,
        bool wipe_backup_gpt,
        base::RepeatingCallback<bool()> is_cancelled,
        base::RepeatingCallback<void(int)> set_progress) = 0;

    // Creates a transfer that reads back the written blocks from the open USB
    // device and compares them byte-for-byte against `image_path`. Requires
    // `Open()` to have succeeded.
    virtual std::unique_ptr<UsbTransfer> CreateVerifyTransfer(
        base::FilePath image_path,
        base::RepeatingCallback<bool()> is_cancelled,
        base::RepeatingCallback<void(int)> set_progress) = 0;
  };

  using UsbDeviceManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::UsbDeviceManager>)>;

  void SetUsbDeviceManagerBinderForTesting(UsbDeviceManagerBinder binder) {
    usb_binder_ = std::move(binder);
  }
#endif

 protected:
  virtual ~Operation();

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

  // This function should be overriden by subclasses to set up the work of the
  // operation.  It will be called from Start().
  virtual void StartImpl() = 0;

  // Returns true if `Write()` over USB Mass Storage should also zero the backup
  // GPT header and partition entries at the end of the disk.
  virtual bool ShouldWipeBackupGpt() const;

  // Extracts the current file if it's an archive.  The current_file will be set
  // to the extracted file.
  void Extract(base::OnceClosure continuation);

  // Writes the current file to device_path.
  void Write(base::OnceClosure continuation);

  // Verifies that the current file and device_path contents match.
  void VerifyWrite(base::OnceClosure continuation);

  // Completes the operation.
  void Finish();

  // Generates an error.
  // `error_message` is used to create an OnWriteError event which is
  // sent to the extension
  void Error(const std::string& error_message);

  // Set `progress_` and send an event.  Progress should be in the interval
  // [0,100]
  void SetProgress(int progress);
  // Change to a new `stage_` and set `progress_` to zero.  Triggers a progress
  // event.
  void SetStage(image_writer_api::Stage stage);

  // Can be queried to safely determine if the operation has been cancelled.
  bool IsCancelled();

  // Adds a callback that will be called during clean-up, whether the operation
  // is aborted, encounters and error, or finishes successfully.  These
  // functions will be run on `task_runner_`.
  void AddCleanUpFunction(base::OnceClosure callback);

  // Completes the current operation (progress set to 100) and runs the
  // continuation.
  void CompleteAndContinue(base::OnceClosure continuation);

  // Computes the lowercase hex MD5 digest of `file_path` on `task_runner_`,
  // updating stage progress from 0 to 100, and passes it to `callback`.
  void GetMD5SumOfFile(const base::FilePath& file_path,
                       base::OnceCallback<void(const std::string&)> callback);

  bool IsRunningInCorrectSequence() const;

  base::WeakPtr<OperationManager> manager_;
  const ExtensionId extension_id_;

  base::FilePath image_path_;
  base::FilePath device_path_;

  // Temporary directory to store files as we go.
  std::unique_ptr<base::ScopedTempDir> temp_dir_;

 private:
  friend class base::RefCountedThreadSafe<Operation>;
  friend class OperationForTest;
  friend class ImageWriterUtilityClientTest;
  friend class WriteFromUrlOperationForTest;

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  // Ensures the client is started.  This may be called many times but will only
  // instantiate one client which should exist for the lifetime of the
  // Operation.
  void StartUtilityClient();

  // Stops the client.  This must be called to ensure the utility process can
  // shutdown.
  void StopUtilityClient();

  // Reports progress from the client, transforming from bytes to percentage.
  virtual void WriteImageProgress(int64_t total_bytes, int64_t curr_bytes);

  scoped_refptr<ImageWriterUtilityClient> image_writer_client_;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Unmounts all volumes on `device_path_`.
  void UnmountVolumes(base::OnceClosure continuation);
  // Starts the write after unmounting.
  void UnmountVolumesCallback(base::OnceClosure continuation,
                              ash::MountError error_code);
  // Starts the ImageBurner write.  Note that target_path is the file path of
  // the device where device_path has been a system device path.
  void StartWriteOnUIThread(const std::string& target_path,
                            base::OnceClosure continuation);
  void OnBurnFinished(base::OnceClosure continuation,
                      const std::string& target_path,
                      bool success,
                      const std::string& error);
  void OnBurnProgress(const std::string& target_path,
                      int64_t num_bytes_burnt,
                      int64_t total_size);
  void OnBurnError();
#endif

#if BUILDFLAG(IS_ANDROID)
  void EnsureUsbConnection(ParsedDeviceId device_id,
                           base::OnceClosure on_connected);
  void OnUsbManagerDisconnected();
  void OnUsbDeviceInfoRefreshed(ParsedDeviceId device_id,
                                base::OnceClosure on_connected,
                                device::mojom::UsbDeviceInfoPtr info);
  void OnUsbConnectionOpened(std::unique_ptr<UsbDeviceConnection> connection,
                             base::OnceClosure on_connected,
                             base::expected<void, std::string> result);
  void StartUsbWrite(base::OnceClosure continuation);
  void StartUsbVerify(base::OnceClosure continuation);
  void OnUsbTransferFinished(base::OnceClosure continuation,
                             base::expected<void, std::string> result);
  void CloseUsbConnection(base::OnceClosure continuation);
  void OnUsbConnectionClosed(std::unique_ptr<UsbDeviceConnection> connection,
                             base::OnceClosure continuation);
#endif

  // Incrementally calculates the MD5 sum of a file.
  void MD5Chunk(base::File file,
                crypto::obsolete::Md5 md5,
                base::HeapArray<uint8_t> buffer,
                int64_t bytes_processed,
                int64_t bytes_total,
                base::OnceCallback<void(const std::string&)> callback);

  // Callbacks for Extractor.
  void OnExtractOpenComplete(const base::FilePath& image_path);

  // Note: `total_bytes` and `progress_bytes` are signed `int64_t `because
  // `ZipExtractor` and `zip::ZipReader` deal in `int64_t` size values.
  // TODO(crbug.com/489798713): Consider refactoring ZipReader to use unsigned
  // values if possible.
  // Extractors that provide `uint64_t` values (like `TarExtractor`) must
  // validate they do not exceed `INT64_MAX` before passing them here to prevent
  // overflow.
  void OnExtractProgress(int64_t total_bytes, int64_t progress_bytes);

  void OnExtractFailure(const std::string& error);

  // Runs all cleanup functions.
  void CleanUp();

  // `stage_` and `progress_` are owned by the FILE thread, use `SetStage` and
  // `SetProgress` to update.  Progress should be in the interval [0,100]
  image_writer_api::Stage stage_ = image_writer_api::Stage::kUnknown;
  int progress_ = 0;

  // Cleanup operations that must be run.  All these functions are run on
  // `task_runner_`.
  std::vector<base::OnceClosure> cleanup_functions_;

  // Active Zstandard extractor, owned on `task_runner_` so it is terminated on
  // cancellation or shutdown.
  std::unique_ptr<ZstdExtractor> zstd_extractor_;

  static constexpr base::TaskTraits blocking_task_traits() {
    return {
        // Requires I/O.
        base::MayBlock(),
        // Apps (e.g. Chromebook Recovery Utility) present UI feedback based on
        // an operation, but it's not on critical path.
        base::TaskPriority::USER_VISIBLE,
        base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
    };
  }

  // The download folder on Chrome OS.
  const base::FilePath download_folder_;

  // Sequenced task runner where all I/O operation will be performed.
  // Most of the methods of this class run in this task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

#if BUILDFLAG(IS_ANDROID)
  UsbDeviceManagerBinder usb_binder_;
  // `usb_manager_`, `usb_connection_` (which stays open across `Write()` and
  // `VerifyWrite()` and is closed in `Finish()` or on error), and
  // `usb_transfer_` are created, used, and reset exclusively on `task_runner_`.
  // `CleanUp()` (called on `task_runner_` by `Finish()`, `Error()`, `Cancel()`,
  // and `Abort()`, including `OperationManager::Shutdown()`) resets them before
  // `this` can be destroyed, and `~Operation()` hands them to
  // `task_runner_->DeleteSoon()`. Their callbacks therefore safely bind
  // `base::Unretained(this)`.
  std::unique_ptr<mojo::Remote<device::mojom::UsbDeviceManager>> usb_manager_;
  std::unique_ptr<UsbDeviceConnection> usb_connection_;
  std::unique_ptr<UsbTransfer> usb_transfer_;
#endif
};

}  // namespace extensions::image_writer

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_OPERATION_H_
