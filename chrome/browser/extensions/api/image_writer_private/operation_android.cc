// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/removable_storage_provider_android.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace extensions::image_writer {

namespace {

constexpr size_t kTransferBufferSize = 64 * 1024;  // 64 KiB

enum class TransferMode { kWrite, kVerify };

struct LocalFileTransferState {
  LocalFileTransferState(base::File image_file,
                         base::File device_file,
                         TransferMode mode,
                         int64_t total_size,
                         size_t chunk_size,
                         base::RepeatingCallback<bool()> is_cancelled,
                         base::RepeatingCallback<void(int)> set_progress,
                         base::OnceCallback<void(
                             base::expected<void, std::string>)> callback)
      : image_file(std::move(image_file)),
        device_file(std::move(device_file)),
        mode(mode),
        total_size(total_size),
        chunk_size(chunk_size),
        image_buffer(chunk_size),
        device_buffer(mode == TransferMode::kVerify ? chunk_size : 0),
        is_cancelled(std::move(is_cancelled)),
        set_progress(std::move(set_progress)),
        callback(std::move(callback)) {}

  base::File image_file;
  base::File device_file;
  const TransferMode mode;
  const int64_t total_size;
  const size_t chunk_size;
  int64_t bytes_processed = 0;
  std::vector<uint8_t> image_buffer;
  std::vector<uint8_t> device_buffer;
  base::RepeatingCallback<bool()> is_cancelled;
  base::RepeatingCallback<void(int)> set_progress;
  base::OnceCallback<void(base::expected<void, std::string>)> callback;
};

// Note: `Operation::Write` and `Operation::VerifyWrite` already execute on
// `Operation::task_runner_` (a background `ThreadPool` `MayBlock()` sequence,
// not the UI thread). Because `OperationManager::CancelWrite()` cancels an
// operation by posting `Operation::Cancel()` onto `Operation::task_runner_` and
// `Operation::IsCancelled()` / `Operation::SetProgress()` `CHECK` that they
// run on `Operation::task_runner_`, each 64 KiB chunk is posted back onto
// `Operation::task_runner_` (matching `Operation::MD5Chunk` in `operation.cc`)
// so a queued `Cancel()` / `Abort()` task can interleave between chunks.
void ProcessLocalFileTransferChunk(
    std::unique_ptr<LocalFileTransferState> state) {
  if (state->is_cancelled.Run()) {
    return;
  }

  const size_t bytes_to_read = static_cast<size_t>(std::min<int64_t>(
      state->chunk_size, state->total_size - state->bytes_processed));
  base::span<uint8_t> read_span =
      base::span(state->image_buffer).first(bytes_to_read);
  const std::optional<size_t> bytes_read =
      state->image_file.ReadAtCurrentPos(read_span);
  if (!bytes_read.has_value() || *bytes_read == 0) {
    std::move(state->callback).Run(base::unexpected(error::kImageReadError));
    return;
  }

  switch (state->mode) {
    case TransferMode::kWrite:
      if (!state->device_file.WriteAtCurrentPosAndCheck(
              read_span.first(*bytes_read))) {
        std::move(state->callback)
            .Run(base::unexpected(error::kDeviceWriteError));
        return;
      }
      break;
    case TransferMode::kVerify: {
      const std::optional<size_t> device_bytes_read =
          state->device_file.ReadAtCurrentPos(
              base::span(state->device_buffer).first(*bytes_read));
      if (!device_bytes_read.has_value()) {
        std::move(state->callback).Run(base::unexpected(error::kHashReadError));
        return;
      }
      if (*device_bytes_read != *bytes_read ||
          read_span.first(*bytes_read) !=
              base::span(state->device_buffer).first(*device_bytes_read)) {
        std::move(state->callback)
            .Run(base::unexpected(error::kVerificationFailed));
        return;
      }
      break;
    }
  }

  state->bytes_processed += static_cast<int64_t>(*bytes_read);
  state->set_progress.Run(static_cast<int>(
      kProgressComplete * state->bytes_processed / state->total_size));

  if (state->bytes_processed >= state->total_size) {
    if (state->mode == TransferMode::kWrite && !state->device_file.Flush()) {
      std::move(state->callback)
          .Run(base::unexpected(error::kDeviceWriteError));
      return;
    }
    std::move(state->callback).Run(base::ok());
    return;
  }

  // `GetCurrentDefault()` is `Operation::task_runner_` (verified by
  // `CHECK(IsRunningInCorrectSequence())` in `Operation::Write` and
  // `Operation::VerifyWrite`).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProcessLocalFileTransferChunk, std::move(state)));
}

// Streams `image_path` into (or compares it against) `device_path` in chunks of
// up to `kTransferBufferSize` posted onto `Operation::task_runner_`.
void StartLocalFileTransfer(
    const base::FilePath& image_path,
    const base::FilePath& device_path,
    TransferMode mode,
    base::RepeatingCallback<bool()> is_cancelled,
    base::RepeatingCallback<void(int)> set_progress,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  base::File image_file(image_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!image_file.IsValid()) {
    std::move(callback).Run(base::unexpected(error::kImageOpenError));
    return;
  }
  const int64_t total_size = image_file.GetLength();
  if (total_size <= 0) {
    std::move(callback).Run(base::unexpected(error::kImageSizeError));
    return;
  }

  const uint32_t device_flags =
      base::File::FLAG_OPEN |
      (mode == TransferMode::kWrite ? base::File::FLAG_WRITE
                                    : base::File::FLAG_READ);
  base::File device_file(device_path, device_flags);
  if (!device_file.IsValid()) {
    std::move(callback).Run(base::unexpected(error::kDeviceOpenError));
    return;
  }

  // Cap `chunk_size` at `total_size / 2` so small test files still emit an
  // intermediate progress event before 100%.
  const size_t chunk_size = static_cast<size_t>(
      std::clamp<int64_t>(total_size / 2, 1, kTransferBufferSize));
  auto state = std::make_unique<LocalFileTransferState>(
      std::move(image_file), std::move(device_file), mode, total_size,
      chunk_size, std::move(is_cancelled), std::move(set_progress),
      std::move(callback));
  ProcessLocalFileTransferChunk(std::move(state));
}

void OnLocalFileTransferComplete(
    base::OnceClosure on_success,
    base::OnceCallback<void(const std::string&)> on_error,
    base::expected<void, std::string> result) {
  if (result.has_value()) {
    std::move(on_success).Run();
  } else {
    std::move(on_error).Run(result.error());
  }
}

}  // namespace

void Operation::ConnectToUsbDevice(ParsedDeviceId device_id,
                                   base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  usb_manager_ =
      std::make_unique<mojo::Remote<device::mojom::UsbDeviceManager>>();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(usb_binder_, usb_manager_->BindNewPipeAndPassReceiver()));
  usb_manager_->set_disconnect_handler(base::BindOnce(
      &Operation::OnUsbManagerDisconnected, base::Unretained(this)));
  std::string guid = device_id.guid;
  (*usb_manager_)
      ->RefreshDeviceInfo(guid,
                          base::BindOnce(&Operation::OnUsbDeviceInfoRefreshed,
                                         base::Unretained(this),
                                         std::move(device_id),
                                         std::move(continuation)));
}

void Operation::OnUsbManagerDisconnected() {
  CHECK(IsRunningInCorrectSequence());
  if (!IsCancelled()) {
    Error(error::kDeviceOpenError);
  }
}

void Operation::OnUsbDeviceInfoRefreshed(
    ParsedDeviceId device_id,
    base::OnceClosure continuation,
    device::mojom::UsbDeviceInfoPtr info) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    usb_manager_.reset();
    return;
  }
  if (!info || info->vendor_id != device_id.vendor_id ||
      info->product_id != device_id.product_id) {
    Error(error::kDeviceOpenError);
    return;
  }
  std::optional<UsbMassStorageEndpoints> endpoints =
      ExtractMassStorageEndpoints(*info);
  if (!endpoints.has_value()) {
    Error(error::kDeviceOpenError);
    return;
  }
  mojo::PendingRemote<device::mojom::UsbDevice> device_remote;
  (*usb_manager_)
      ->GetUnrestrictedDevice(device_id.guid, {},
                              device_remote.InitWithNewPipeAndPassReceiver(),
                              mojo::NullRemote());
  // Safe: Mojo allows destroying the Remote from within its response callback.
  usb_manager_.reset();
  OnUsbDeviceConnected(std::move(continuation), *endpoints,
                       std::move(device_remote));
}

void Operation::OnUsbDeviceConnected(
    base::OnceClosure continuation,
    const UsbMassStorageEndpoints& endpoints,
    mojo::PendingRemote<device::mojom::UsbDevice> device_remote) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }
  if (!device_remote.is_valid()) {
    Error(error::kDeviceOpenError);
    return;
  }
  // TODO(crbug.com/538632205): Open `MojoUsbBotSession` and stream image blocks
  // in (4b/6) and (4c/6). Fail closed until the BOT transport is wired up so a
  // USB target never reports fake completion with zero bytes written.
  Error(error::kDeviceWriteError);
}

void Operation::Write(base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }
  SetStage(image_writer_api::Stage::kWrite);

  if (std::optional<ParsedDeviceId> parsed_id =
          ParseUsbDeviceId(device_path_.value())) {
    ConnectToUsbDevice(std::move(*parsed_id), std::move(continuation));
    return;
  }

  StartLocalFileTransfer(image_path_, device_path_, TransferMode::kWrite,
                         base::BindRepeating(&Operation::IsCancelled, this),
                         base::BindRepeating(&Operation::SetProgress, this),
                         base::BindOnce(
                             &OnLocalFileTransferComplete,
                             base::BindOnce(&Operation::CompleteAndContinue,
                                            this, std::move(continuation)),
                             base::BindOnce(&Operation::Error, this)));
}

void Operation::VerifyWrite(base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }
  SetStage(image_writer_api::Stage::kVerifyWrite);

  if (std::optional<ParsedDeviceId> parsed_id =
          ParseUsbDeviceId(device_path_.value())) {
    // TODO(crbug.com/538632205): Verify written USB blocks via SCSI READ (10)
    // in (5/6).
    Error(error::kVerificationFailed);
    return;
  }

  StartLocalFileTransfer(image_path_, device_path_, TransferMode::kVerify,
                         base::BindRepeating(&Operation::IsCancelled, this),
                         base::BindRepeating(&Operation::SetProgress, this),
                         base::BindOnce(
                             &OnLocalFileTransferComplete,
                             base::BindOnce(&Operation::CompleteAndContinue,
                                            this, std::move(continuation)),
                             base::BindOnce(&Operation::Error, this)));
}

}  // namespace extensions::image_writer
