// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <algorithm>
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

void Operation::Write(base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }
  SetStage(image_writer_api::Stage::kWrite);
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
