// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
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
static_assert(
    std::has_single_bit(kTransferBufferSize),
    "kTransferBufferSize must be a power of two so it is a multiple of any "
    "accepted power-of-two block size <= kTransferBufferSize.");
// A standard GPT uses 1 block for the GPT header plus 32 blocks for 128
// partition entries of 128 bytes each at 512 bytes/sector (33 blocks total; at
// 4 KiB sectors this is a safe over-approximation).
constexpr uint16_t kBackupGptBlocks = 33;
constexpr uint32_t kDefaultBlockSize = 512;
constexpr base::TimeDelta kUsbCommandTimeout = base::Seconds(10);
// Flushing a multi-gigabyte image to slow USB flash media can take longer than
// standard 10s block transfers.
constexpr base::TimeDelta kUsbSyncCacheTimeout = base::Seconds(60);

enum class TransferMode { kWrite, kVerify };

// Describes the optional data phase of a USB Mass Storage Bulk-Only Transport
// (BOT) transaction:
// - `NoDataPhase`: Command has no data transfer phase
//   (`dCBWDataTransferLength == 0`, `bmCBWFlags == 0x00`).
// - `DataInPhase`: Device-to-host transfer of `length` bytes over bulk-IN
//   (`dCBWDataTransferLength == length`, `bmCBWFlags == 0x80`).
// - `DataOutPhase`: Host-to-device transfer of `data` over bulk-OUT
//   (`dCBWDataTransferLength == data.size()`, `bmCBWFlags == 0x00`).
struct NoDataPhase {};
struct DataInPhase {
  uint32_t length = 0;
};
struct DataOutPhase {
  base::raw_span<const uint8_t> data;
};
using DataPhase = std::variant<NoDataPhase, DataInPhase, DataOutPhase>;

struct Capacity10Response {
  uint32_t last_logical_block_address = 0;
  uint32_t block_size = 0;
};

// Parses the 8-byte big-endian parameter data returned by the SCSI
// `READ CAPACITY (10)` command (last logical block address + block size in
// bytes). Returns `std::nullopt` if the response is truncated, reports
// 0xFFFFFFFF (`READ CAPACITY (16)` required for >2 TiB media;
// TODO(crbug.com/538632205)), or reports an invalid block size.
std::optional<Capacity10Response> ParseReadCapacity10Response(
    base::span<const uint8_t> data) {
  if (data.size() < kScsiReadCapacity10ResponseLength) {
    return std::nullopt;
  }
  const uint32_t parsed_last_block = base::U32FromBigEndian(data.first<4>());
  const uint32_t parsed_block_size =
      base::U32FromBigEndian(data.subspan<4, 4>());
  if (parsed_last_block == 0xFFFFFFFF || parsed_last_block == 0 ||
      parsed_block_size < kDefaultBlockSize ||
      parsed_block_size > kTransferBufferSize ||
      !std::has_single_bit(parsed_block_size)) {
    return std::nullopt;
  }
  return Capacity10Response{.last_logical_block_address = parsed_last_block,
                            .block_size = parsed_block_size};
}

// Builds a 10-byte SCSI `WRITE (10)` (opcode 0x2A) or `READ (10)` (opcode 0x28)
// command descriptor block:
// - Byte 0: Operation code
// - Bytes 2..5: Big-endian 32-bit starting Logical Block Address (LBA)
// - Bytes 7..8: Big-endian 16-bit transfer length in logical blocks
std::vector<uint8_t> BuildScsiReadWrite10Command(uint8_t opcode,
                                                 uint32_t logical_block_address,
                                                 uint16_t num_blocks) {
  std::vector<uint8_t> command(10, 0);
  base::SpanWriter<uint8_t> writer(command);
  writer.WriteU8LittleEndian(opcode);
  writer.WriteU8LittleEndian(0);
  writer.WriteU32BigEndian(logical_block_address);
  writer.WriteU8LittleEndian(0);
  writer.WriteU16BigEndian(num_blocks);
  writer.WriteU8LittleEndian(0);
  return command;
}

// Truncates `buffer` to `bytes_read` (discarding any unread trailing bytes
// when `bytes_read < buffer->size()`) and, if `bytes_read` is not already a
// multiple of `block_size`, pads `buffer` with trailing zeros up to the next
// multiple of `block_size`, returning the block-aligned byte length.
size_t AlignAndZeroPad(std::vector<uint8_t>* buffer,
                       size_t bytes_read,
                       uint32_t block_size) {
  buffer->resize(bytes_read);
  const size_t remainder = bytes_read % block_size;
  if (remainder == 0) {
    return bytes_read;
  }
  const size_t aligned_size = bytes_read + (block_size - remainder);
  buffer->resize(aligned_size, 0);
  return aligned_size;
}

// Sends raw USB Mass Storage Bulk-Only Transport (BOT) command transactions
// (`CBW` -> optional data phase with `STALL` recovery -> `CSW`) over
// `device::mojom::UsbDevice`.
class UsbBotTransport {
 public:
  using TransactionCallback =
      base::OnceCallback<void(std::optional<uint8_t> csw_status,
                              std::vector<uint8_t> received_data)>;

  UsbBotTransport(device::mojom::UsbDevice* device,
                  UsbMassStorageEndpoints endpoints)
      : device_(device), endpoints_(endpoints) {}

  UsbBotTransport(const UsbBotTransport&) = delete;
  UsbBotTransport& operator=(const UsbBotTransport&) = delete;
  ~UsbBotTransport() = default;

  // Any `DataOutPhase::data` span must remain valid until `callback` is invoked
  // (or `this` is destroyed).
  void ExecuteTransaction(base::span<const uint8_t> scsi_command,
                          DataPhase data_phase,
                          base::TimeDelta timeout,
                          TransactionCallback callback) {
    CHECK(callback);
    CHECK(!callback_);
    tag_ = ++next_tag_;
    data_phase_ = data_phase;
    timeout_ms_ = static_cast<uint32_t>(timeout.InMilliseconds());
    received_data_.clear();
    callback_ = std::move(callback);

    uint32_t data_length = 0;
    uint8_t direction_flags = kDirectionHostToDevice;
    if (const auto* data_in = std::get_if<DataInPhase>(&data_phase_)) {
      CHECK_GT(data_in->length, 0u);
      data_length = data_in->length;
      direction_flags = kDirectionDeviceToHost;
    } else if (const auto* data_out = std::get_if<DataOutPhase>(&data_phase_)) {
      CHECK(!data_out->data.empty());
      data_length = base::checked_cast<uint32_t>(data_out->data.size());
      direction_flags = kDirectionHostToDevice;
    }

    std::vector<uint8_t> command_block = BuildCommandBlockWrapper(
        tag_, data_length, direction_flags, scsi_command);
    device_->GenericTransferOut(
        endpoints_.endpoint_out, command_block, timeout_ms_,
        base::BindOnce(&UsbBotTransport::OnCommandBlockSent,
                       base::Unretained(this)));
  }

 private:
  void OnCommandBlockSent(device::mojom::UsbTransferStatus status) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(std::nullopt, {});
    }
    if (const auto* data_in = std::get_if<DataInPhase>(&data_phase_)) {
      device_->GenericTransferIn(
          endpoints_.endpoint_in, data_in->length, timeout_ms_,
          base::BindOnce(&UsbBotTransport::OnDataInReceived,
                         base::Unretained(this)));
      return;
    }
    if (const auto* data_out = std::get_if<DataOutPhase>(&data_phase_)) {
      device_->GenericTransferOut(
          endpoints_.endpoint_out, data_out->data, timeout_ms_,
          base::BindOnce(&UsbBotTransport::OnDataOutSent,
                         base::Unretained(this)));
      return;
    }
    ReadCommandStatus();
  }

  void OnDataInReceived(device::mojom::UsbTransferStatus status,
                        base::span<const uint8_t> data) {
    if (status == device::mojom::UsbTransferStatus::STALLED) {
      received_data_.assign(data.begin(), data.end());
      device_->ClearHalt(device::mojom::UsbTransferDirection::INBOUND,
                         endpoints_.endpoint_in,
                         base::BindOnce(&UsbBotTransport::OnDataStallCleared,
                                        base::Unretained(this)));
      return;
    }
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(std::nullopt, {});
    }
    received_data_.assign(data.begin(), data.end());
    ReadCommandStatus();
  }

  void OnDataOutSent(device::mojom::UsbTransferStatus status) {
    data_phase_ = NoDataPhase{};
    if (status == device::mojom::UsbTransferStatus::STALLED) {
      device_->ClearHalt(device::mojom::UsbTransferDirection::OUTBOUND,
                         endpoints_.endpoint_out,
                         base::BindOnce(&UsbBotTransport::OnDataStallCleared,
                                        base::Unretained(this)));
      return;
    }
    if (status != device::mojom::UsbTransferStatus::COMPLETED) {
      return std::move(callback_).Run(std::nullopt, {});
    }
    ReadCommandStatus();
  }

  void OnDataStallCleared(bool success) {
    if (!success) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(std::nullopt, {});
    }
    ReadCommandStatus();
  }

  void ReadCommandStatus() {
    data_phase_ = NoDataPhase{};
    device_->GenericTransferIn(
        endpoints_.endpoint_in, kCommandStatusWrapperSize, timeout_ms_,
        base::BindOnce(&UsbBotTransport::OnCommandStatusReceived,
                       base::Unretained(this)));
  }

  void OnCommandStatusReceived(device::mojom::UsbTransferStatus status,
                               base::span<const uint8_t> status_bytes) {
    // TODO(crbug.com/538632205): Per BOT 1.0 §6.7.2 / Figure 2, handle a
    // `STALLED` status on the CSW read itself with `ClearHalt(INBOUND)` and a
    // single retry of the CSW read before falling back to Reset Recovery.
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      return std::move(callback_).Run(std::nullopt, {});
    }
    std::optional<CommandStatusWrapper> status_wrapper =
        ParseCommandStatusWrapper(status_bytes);
    if (!status_wrapper ||
        status_wrapper->signature != kCommandStatusWrapperSignature ||
        status_wrapper->tag != tag_) {
      return std::move(callback_).Run(std::nullopt, {});
    }
    std::move(callback_).Run(status_wrapper->status, std::move(received_data_));
  }

  const raw_ptr<device::mojom::UsbDevice> device_;
  const UsbMassStorageEndpoints endpoints_;
  uint32_t next_tag_ = 0;
  uint32_t tag_ = 0;
  DataPhase data_phase_;
  uint32_t timeout_ms_ = 0;
  std::vector<uint8_t> received_data_;
  TransactionCallback callback_;
};

// Executes SCSI commands over `UsbBotTransport`, automatically issuing
// `REQUEST SENSE` on `CHECK CONDITION` (CSW status 1) and retrying when the
// sense data reports a transient `UNIT ATTENTION` or `NOT READY` (becoming
// ready) condition.
class UsbScsiCommandRunner {
 public:
  using CommandCallback =
      base::OnceCallback<void(bool ok, std::vector<uint8_t> data)>;

  UsbScsiCommandRunner(device::mojom::UsbDevice* device,
                       UsbMassStorageEndpoints endpoints)
      : transport_(device, endpoints) {}

  UsbScsiCommandRunner(const UsbScsiCommandRunner&) = delete;
  UsbScsiCommandRunner& operator=(const UsbScsiCommandRunner&) = delete;
  ~UsbScsiCommandRunner() = default;

  // Any `DataOutPhase::data` span must remain valid until `callback` is invoked
  // (or `this` is destroyed).
  void SendCommandWithRetry(std::vector<uint8_t> scsi_command,
                            DataPhase data_phase,
                            int attempts_left,
                            base::TimeDelta timeout,
                            CommandCallback callback) {
    CHECK(callback);
    CHECK(!callback_);
    scsi_command_ = std::move(scsi_command);
    data_phase_ = data_phase;
    attempts_left_ = attempts_left;
    timeout_ = timeout;
    last_sense_key_ = 0;
    callback_ = std::move(callback);
    AttemptCommand();
  }

  uint8_t last_sense_key() const { return last_sense_key_; }

 private:
  void AttemptCommand() {
    transport_.ExecuteTransaction(
        scsi_command_, data_phase_, timeout_,
        base::BindOnce(&UsbScsiCommandRunner::OnCommandTransactionDone,
                       base::Unretained(this)));
  }

  void OnCommandTransactionDone(std::optional<uint8_t> csw_status,
                                std::vector<uint8_t> received_data) {
    if (!csw_status.has_value()) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(false, {});
    }
    if (*csw_status == 0) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(true, std::move(received_data));
    }
    // TODO(crbug.com/538632205): Perform Bulk-Only Mass Storage Reset and
    // ClearHalt recovery when `csw_status == 2` (Phase Error).
    if (*csw_status != 1) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(false, {});
    }
    // Issue `REQUEST SENSE` (opcode 0x03) on `CHECK CONDITION` (status 1) to
    // inspect the SCSI sense key / ASC / ASCQ and clear unit attention.
    static constexpr std::array<uint8_t, 6> kRequestSenseCommand = {
        kScsiRequestSenseOpcode,
        0,
        0,
        0,
        static_cast<uint8_t>(kScsiRequestSenseAllocationLength),
        0};
    transport_.ExecuteTransaction(
        kRequestSenseCommand, DataInPhase{kScsiRequestSenseAllocationLength},
        kUsbCommandTimeout,
        base::BindOnce(&UsbScsiCommandRunner::OnRequestSenseTransactionDone,
                       base::Unretained(this)));
  }

  void OnRequestSenseTransactionDone(std::optional<uint8_t> csw_status,
                                     std::vector<uint8_t> sense_data) {
    // USB Mass Storage devices return fixed-format sense data (response code
    // 0x70 for current errors or 0x71 for deferred errors), where the sense key
    // is in byte 2 (bits 0..3) and ASC / ASCQ are in bytes 12 and 13.
    if (csw_status != 0 || sense_data.size() < 14) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(false, {});
    }
    const uint8_t response_code = sense_data[0] & 0x7F;
    if (response_code != 0x70 && response_code != 0x71) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(false, {});
    }
    const uint8_t sense_key = sense_data[2] & 0x0F;
    const uint8_t asc = sense_data[12];
    const uint8_t ascq = sense_data[13];
    last_sense_key_ = sense_key;

    const bool is_retryable =
        sense_key == kScsiSenseKeyUnitAttention ||
        (sense_key == kScsiSenseKeyNotReady &&
         asc == kScsiAscLogicalUnitNotReady && ascq == kScsiAscqBecomingReady);
    if (!is_retryable || attempts_left_ <= 1) {
      data_phase_ = NoDataPhase{};
      return std::move(callback_).Run(false, {});
    }
    --attempts_left_;
    AttemptCommand();
  }

  UsbBotTransport transport_;
  std::vector<uint8_t> scsi_command_;
  DataPhase data_phase_;
  int attempts_left_ = 0;
  base::TimeDelta timeout_ = kUsbCommandTimeout;
  uint8_t last_sense_key_ = 0;
  CommandCallback callback_;
};

// Streams an image file to (or verifies it against) a USB Mass Storage
// Bulk-Only Transport device over `device::mojom::UsbDevice` on
// `Operation::task_runner_`.
class MojoUsbBotSession : public Operation::UsbSession {
 public:
  MojoUsbBotSession(base::FilePath image_path,
                    TransferMode mode,
                    bool wipe_backup_gpt,
                    UsbMassStorageEndpoints endpoints,
                    mojo::PendingRemote<device::mojom::UsbDevice> device_remote,
                    base::RepeatingCallback<bool()> is_cancelled,
                    base::RepeatingCallback<void(int)> set_progress)
      : image_path_(std::move(image_path)),
        mode_(mode),
        wipe_backup_gpt_(wipe_backup_gpt),
        endpoints_(endpoints),
        device_(std::move(device_remote)),
        command_runner_(device_.get(), endpoints_),
        is_cancelled_(std::move(is_cancelled)),
        set_progress_(std::move(set_progress)) {}

  MojoUsbBotSession(const MojoUsbBotSession&) = delete;
  MojoUsbBotSession& operator=(const MojoUsbBotSession&) = delete;
  ~MojoUsbBotSession() override = default;

  void Start(CompletionCallback callback) override {
    CHECK(callback);
    CHECK(!callback_);
    callback_ = std::move(callback);
    // Defensive check: `Operation::Cancel()` and `Abort()` run `CleanUp()` on
    // `task_runner_` and destroy `usb_session_` synchronously.
    if (is_cancelled_.Run()) {
      return Finish(base::unexpected(error::kAborted));
    }
    image_file_.Initialize(image_path_,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!image_file_.IsValid()) {
      return Finish(base::unexpected(error::kImageOpenError));
    }
    total_size_ = image_file_.GetLength();
    if (total_size_ <= 0) {
      return Finish(base::unexpected(error::kImageSizeError));
    }

    device_.set_disconnect_handler(base::BindOnce(
        &MojoUsbBotSession::OnDisconnected, base::Unretained(this)));
    device_->Open(
        base::BindOnce(&MojoUsbBotSession::OnOpened, base::Unretained(this)));
  }

 private:
  void OnOpened(device::mojom::UsbOpenDeviceResultPtr result) {
    if (!result || !result->is_success()) {
      return Finish(base::unexpected(error::kDeviceOpenError));
    }
    opened_ = true;
    device_->ClaimInterface(
        endpoints_.interface_number,
        base::BindOnce(&MojoUsbBotSession::OnClaimed, base::Unretained(this)));
  }

  void OnClaimed(device::mojom::UsbClaimInterfaceResult result) {
    if (result != device::mojom::UsbClaimInterfaceResult::kSuccess) {
      return Finish(base::unexpected(error::kDeviceOpenError));
    }
    claimed_ = true;
    // Send SCSI `TEST UNIT READY` (0x00) to clear any initial `UNIT ATTENTION`
    // condition before querying block size and writing blocks.
    SendCommandWithRetry({kScsiTestUnitReadyOpcode, 0, 0, 0, 0, 0},
                         NoDataPhase{}, /*attempts_left=*/3, kUsbCommandTimeout,
                         base::BindOnce(&MojoUsbBotSession::OnTestUnitReadyDone,
                                        base::Unretained(this)));
  }

  void OnTestUnitReadyDone(bool ok, std::vector<uint8_t>) {
    if (!ok) {
      return Finish(base::unexpected(error::kDeviceOpenError));
    }
    SendCommandWithRetry({kScsiReadCapacity10Opcode, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                         DataInPhase{kScsiReadCapacity10ResponseLength},
                         /*attempts_left=*/3, kUsbCommandTimeout,
                         base::BindOnce(&MojoUsbBotSession::OnReadCapacityDone,
                                        base::Unretained(this)));
  }

  void OnReadCapacityDone(bool ok, std::vector<uint8_t> data) {
    std::optional<Capacity10Response> capacity =
        ok ? ParseReadCapacity10Response(data) : std::nullopt;
    if (!capacity.has_value()) {
      return Finish(base::unexpected(error::kDeviceOpenError));
    }
    last_block_index_ = capacity->last_logical_block_address;
    block_size_ = capacity->block_size;
    const uint64_t device_capacity_bytes =
        (static_cast<uint64_t>(last_block_index_) + 1) *
        static_cast<uint64_t>(block_size_);
    // Return `kDeviceWriteError` (or `kHashReadError` during verification) when
    // the image exceeds the target device's capacity to match the desktop
    // ImageWriter implementation (and what the Chromebook Recovery Utility
    // extension expects during the write stage).
    if (static_cast<uint64_t>(total_size_) > device_capacity_bytes) {
      return Finish(base::unexpected(mode_ == TransferMode::kWrite
                                         ? error::kDeviceWriteError
                                         : error::kHashReadError));
    }
    // `ParseReadCapacity10Response()` rejects `block_size >
    // kTransferBufferSize`.
    CHECK_LE(block_size_, kTransferBufferSize);
    // Cap `chunk_size_` at `total_size_ / 2` so small test images still emit an
    // intermediate progress event before 100%.
    const size_t clamped_half = static_cast<size_t>(
        std::clamp<int64_t>(total_size_ / 2, block_size_, kTransferBufferSize));
    chunk_size_ = std::max<size_t>(block_size_,
                                   (clamped_half / block_size_) * block_size_);
    ProcessNextChunk();
  }

  void ProcessNextChunk() {
    // Defensive check: `Operation::Cancel()` and `Abort()` run `CleanUp()` on
    // `task_runner_` and destroy `usb_session_` synchronously.
    if (is_cancelled_.Run()) {
      return Finish(base::unexpected(error::kAborted));
    }
    if (bytes_processed_ >= total_size_) {
      if (mode_ != TransferMode::kWrite) {
        // Verification compares only the first `total_size_` bytes against
        // `image_path_`; the best-effort backup GPT wipe region at the end of
        // the disk (when `wipe_backup_gpt_` is set) is not verified.
        return Finish(base::ok());
      }
      // Only wipe the backup GPT if requested by `DestroyPartitionsOperation`
      // and the disk is large enough that the trailing `kBackupGptBlocks`
      // blocks do not overlap the primary partition table written at the start
      // of the disk.
      const uint32_t primary_blocks_written = current_block_index_;
      if (wipe_backup_gpt_ && !wrote_backup_gpt_ &&
          last_block_index_ >= primary_blocks_written + kBackupGptBlocks) {
        wrote_backup_gpt_ = true;
        current_chunk_.assign(
            static_cast<size_t>(kBackupGptBlocks) * block_size_, 0);
        const uint32_t backup_gpt_start =
            last_block_index_ - (kBackupGptBlocks - 1);
        SendCommandWithRetry(
            BuildScsiReadWrite10Command(kScsiWrite10Opcode, backup_gpt_start,
                                        kBackupGptBlocks),
            DataOutPhase{current_chunk_}, /*attempts_left=*/1,
            kUsbCommandTimeout,
            base::BindOnce(&MojoUsbBotSession::OnBackupGptWiped,
                           base::Unretained(this)));
        return;
      }
      SendCommandWithRetry(
          {kScsiSynchronizeCache10Opcode, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          NoDataPhase{}, /*attempts_left=*/3, kUsbSyncCacheTimeout,
          base::BindOnce(&MojoUsbBotSession::OnSynchronizeCacheDone,
                         base::Unretained(this)));
      return;
    }

    const size_t bytes_to_read = std::min<size_t>(
        chunk_size_, static_cast<size_t>(total_size_ - bytes_processed_));
    current_chunk_.resize(bytes_to_read);
    const std::optional<size_t> bytes_read =
        image_file_.ReadAtCurrentPos(current_chunk_);
    if (!bytes_read.has_value() || *bytes_read == 0) {
      return Finish(base::unexpected(error::kImageReadError));
    }

    const size_t aligned_size =
        AlignAndZeroPad(&current_chunk_, *bytes_read, block_size_);
    const uint16_t num_blocks =
        static_cast<uint16_t>(aligned_size / block_size_);
    const bool is_write = (mode_ == TransferMode::kWrite);
    SendCommandWithRetry(
        BuildScsiReadWrite10Command(
            is_write ? kScsiWrite10Opcode : kScsiRead10Opcode,
            current_block_index_, num_blocks),
        is_write ? DataPhase(DataOutPhase{current_chunk_})
                 : DataPhase(DataInPhase{static_cast<uint32_t>(aligned_size)}),
        /*attempts_left=*/3, kUsbCommandTimeout,
        base::BindOnce(&MojoUsbBotSession::OnChunkDone, base::Unretained(this),
                       num_blocks, *bytes_read));
  }

  void OnChunkDone(uint16_t num_blocks,
                   size_t unaligned_bytes,
                   bool ok,
                   std::vector<uint8_t> device_data) {
    if (mode_ == TransferMode::kWrite) {
      if (!ok) {
        return Finish(base::unexpected(error::kDeviceWriteError));
      }
    } else {
      if (!ok || device_data.size() < unaligned_bytes) {
        // TODO(crbug.com/538632205): Consider introducing a dedicated device
        // read error constant (or reporting `error::kDeviceOpenError`) rather
        // than reusing `error::kHashReadError` on transport/CSW read failures.
        return Finish(base::unexpected(error::kHashReadError));
      }
      if (base::span(current_chunk_).first(unaligned_bytes) !=
          base::span(device_data).first(unaligned_bytes)) {
        return Finish(base::unexpected(error::kVerificationFailed));
      }
    }
    current_block_index_ += num_blocks;
    bytes_processed_ += static_cast<int64_t>(unaligned_bytes);
    set_progress_.Run(
        static_cast<int>(kProgressComplete * bytes_processed_ / total_size_));
    ProcessNextChunk();
  }

  void OnBackupGptWiped(bool ok, std::vector<uint8_t>) {
    if (!ok) {
      return Finish(base::unexpected(error::kDeviceWriteError));
    }
    ProcessNextChunk();
  }

  void OnSynchronizeCacheDone(bool ok, std::vector<uint8_t>) {
    // Many USB flash drives do not implement `SYNCHRONIZE CACHE (10)` and
    // report `CHECK CONDITION` with `ILLEGAL REQUEST` (sense key 0x05); treat
    // that specific sense key as non-fatal, but fail the write on any other
    // flush error.
    if (!ok &&
        command_runner_.last_sense_key() != kScsiSenseKeyIllegalRequest) {
      return Finish(base::unexpected(error::kDeviceWriteError));
    }
    Finish(base::ok());
  }

  void SendCommandWithRetry(std::vector<uint8_t> scsi_command,
                            DataPhase data_phase,
                            int attempts_left,
                            base::TimeDelta timeout,
                            UsbScsiCommandRunner::CommandCallback callback) {
    // Defensive check: `Operation::Cancel()` and `Abort()` run `CleanUp()` on
    // `task_runner_` and destroy `usb_session_` synchronously.
    if (is_cancelled_.Run()) {
      return Finish(base::unexpected(error::kAborted));
    }
    command_runner_.SendCommandWithRetry(std::move(scsi_command), data_phase,
                                         attempts_left, timeout,
                                         std::move(callback));
  }

  void OnDisconnected() {
    CHECK(!finished_);
    claimed_ = false;
    opened_ = false;
    image_file_.Close();
    finished_ = true;
    ReportResult(base::unexpected(error::kDeviceOpenError));
  }

  void Finish(base::expected<void, std::string> result) {
    CHECK(!finished_);
    finished_ = true;
    image_file_.Close();
    if (!device_.is_connected()) {
      return ReportResult(std::move(result));
    }
    device_.set_disconnect_handler(base::BindOnce(
        &MojoUsbBotSession::ReportResult, base::Unretained(this), result));
    if (claimed_) {
      claimed_ = false;
      device_->ReleaseInterface(
          endpoints_.interface_number,
          base::BindOnce(&MojoUsbBotSession::OnReleased, base::Unretained(this),
                         std::move(result)));
      return;
    }
    OnReleased(std::move(result), true);
  }

  void OnReleased(base::expected<void, std::string> result, bool /*success*/) {
    if (opened_ && device_.is_connected()) {
      opened_ = false;
      device_->Close(base::BindOnce(&MojoUsbBotSession::OnClosed,
                                    base::Unretained(this), std::move(result)));
      return;
    }
    opened_ = false;
    ReportResult(std::move(result));
  }

  void OnClosed(base::expected<void, std::string> result) {
    ReportResult(std::move(result));
  }

  // Invokes `callback_` (`Operation::OnUsbBotSessionFinished`), which resets
  // `Operation::usb_session_` and destroys `this`; callers must be in tail
  // position and must not touch any members afterwards.
  void ReportResult(base::expected<void, std::string> result) {
    if (callback_) {
      std::move(callback_).Run(std::move(result));
      // `this` has been destroyed.
    }
  }

  const base::FilePath image_path_;
  const TransferMode mode_;
  const bool wipe_backup_gpt_;
  const UsbMassStorageEndpoints endpoints_;
  mojo::Remote<device::mojom::UsbDevice> device_;
  std::vector<uint8_t> current_chunk_;
  UsbScsiCommandRunner command_runner_;
  base::RepeatingCallback<bool()> is_cancelled_;
  base::RepeatingCallback<void(int)> set_progress_;
  CompletionCallback callback_;
  base::File image_file_;
  int64_t total_size_ = 0;
  int64_t bytes_processed_ = 0;
  uint32_t block_size_ = kDefaultBlockSize;
  // 0-based logical block indices (in units of `block_size_`-byte logical
  // blocks, not bytes).
  uint32_t last_block_index_ = 0;
  uint32_t current_block_index_ = 0;
  size_t chunk_size_ = kTransferBufferSize;
  bool opened_ = false;
  bool claimed_ = false;
  bool finished_ = false;
  bool wrote_backup_gpt_ = false;
};

struct LocalFileTransferState {
  LocalFileTransferState(
      base::File image_file,
      base::File device_file,
      TransferMode mode,
      int64_t total_size,
      size_t chunk_size,
      base::RepeatingCallback<bool()> is_cancelled,
      base::RepeatingCallback<void(int)> set_progress,
      base::OnceCallback<void(base::expected<void, std::string>)> callback)
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
      ->RefreshDeviceInfo(
          guid, base::BindOnce(&Operation::OnUsbDeviceInfoRefreshed,
                               base::Unretained(this), std::move(device_id),
                               std::move(continuation)));
}

void Operation::OnUsbManagerDisconnected() {
  CHECK(IsRunningInCorrectSequence());
  if (!IsCancelled()) {
    Error(error::kDeviceOpenError);
  }
}

void Operation::OnUsbDeviceInfoRefreshed(ParsedDeviceId device_id,
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
  const TransferMode mode = stage_ == image_writer_api::Stage::kVerifyWrite
                                ? TransferMode::kVerify
                                : TransferMode::kWrite;
  const bool wipe_backup_gpt =
      mode == TransferMode::kWrite && ShouldWipeBackupGpt();
  usb_session_ = std::make_unique<MojoUsbBotSession>(
      image_path_, mode, wipe_backup_gpt, endpoints, std::move(device_remote),
      base::BindRepeating(&Operation::IsCancelled, base::Unretained(this)),
      base::BindRepeating(&Operation::SetProgress, base::Unretained(this)));
  usb_session_->Start(base::BindOnce(&Operation::OnUsbBotSessionFinished,
                                     base::Unretained(this),
                                     std::move(continuation)));
}

void Operation::OnUsbBotSessionFinished(
    base::OnceClosure continuation,
    base::expected<void, std::string> result) {
  CHECK(IsRunningInCorrectSequence());
  usb_session_.reset();
  if (IsCancelled()) {
    return;
  }
  if (!result.has_value()) {
    Error(result.error());
    return;
  }
  CompleteAndContinue(std::move(continuation));
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

  StartLocalFileTransfer(
      image_path_, device_path_, TransferMode::kWrite,
      base::BindRepeating(&Operation::IsCancelled, this),
      base::BindRepeating(&Operation::SetProgress, this),
      base::BindOnce(&OnLocalFileTransferComplete,
                     base::BindOnce(&Operation::CompleteAndContinue, this,
                                    std::move(continuation)),
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
    // Note: Opening a second `MojoUsbBotSession` after `Write()` closes its
    // handle relies on `ClaimInterface()` detaching `usb-storage` via
    // `USBDEVFS_DISCONNECT_CLAIM` if the kernel re-probed the partition table
    // between sessions.
    // TODO(crbug.com/538632205): Consider retaining a single
    // `MojoUsbBotSession` across `Write()` and `VerifyWrite()` on `Operation`
    // to avoid re-opening the device.
    ConnectToUsbDevice(std::move(*parsed_id), std::move(continuation));
    return;
  }

  StartLocalFileTransfer(
      image_path_, device_path_, TransferMode::kVerify,
      base::BindRepeating(&Operation::IsCancelled, this),
      base::BindRepeating(&Operation::SetProgress, this),
      base::BindOnce(&OnLocalFileTransferComplete,
                     base::BindOnce(&Operation::CompleteAndContinue, this,
                                    std::move(continuation)),
                     base::BindOnce(&Operation::Error, this)));
}

}  // namespace extensions::image_writer
