// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
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

constexpr uint32_t kDefaultBlockSize = 512;
constexpr size_t kUsb3TransferBufferSize = 512 * 1024;  // 512 KiB
// USB 2.0 drives use smaller transfers: Linux usb-storage caps them at 240
// sectors because some older bridges fail near 256 (USB 3 drives get 2048).
constexpr size_t kUsb2TransferBufferSize = 64 * 1024;  // 64 KiB
// Bulk endpoints report wMaxPacketSize 1024 on USB 3 and 512 on USB 2.0.
constexpr uint32_t kUsb3BulkMaxPacketSize = 1024;
static_assert(
    std::has_single_bit(kUsb3TransferBufferSize) &&
        std::has_single_bit(kUsb2TransferBufferSize) &&
        kUsb2TransferBufferSize <= kUsb3TransferBufferSize,
    "Transfer buffer sizes must be powers of two so they are multiples of any "
    "accepted power-of-two block size.");
static_assert(
    kUsb3TransferBufferSize / kDefaultBlockSize <=
        std::numeric_limits<uint16_t>::max(),
    "kUsb3TransferBufferSize must fit within the 16-bit block count of SCSI "
    "READ (10) and WRITE (10).");
// A standard GPT uses 1 block for the GPT header plus 32 blocks for 128
// partition entries of 128 bytes each at 512 bytes/sector (33 blocks total; at
// 4 KiB sectors this is a safe over-approximation).
constexpr uint16_t kBackupGptBlocks = 33;
constexpr base::TimeDelta kUsbCommandTimeout = base::Seconds(10);
// Flushing a multi-gigabyte image to slow USB flash media can take longer than
// standard 10s block transfers.
constexpr base::TimeDelta kUsbSyncCacheTimeout = base::Seconds(60);

enum class TransferMode { kWrite, kVerify };

// Describes the optional data phase of a USB Mass Storage Bulk-Only Transport
// (BOT) transaction:
// - `std::monostate`: Idle placeholder when no transaction is in progress.
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
using DataPhase =
    std::variant<std::monostate, NoDataPhase, DataInPhase, DataOutPhase>;

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
    base::span<const uint8_t> data,
    size_t max_transfer_size) {
  if (data.size() < kScsiReadCapacity10ResponseLength) {
    return std::nullopt;
  }
  const uint32_t parsed_last_block = base::U32FromBigEndian(data.first<4>());
  const uint32_t parsed_block_size =
      base::U32FromBigEndian(data.subspan<4, 4>());
  if (parsed_last_block == 0xFFFFFFFF || parsed_last_block == 0 ||
      parsed_block_size < kDefaultBlockSize ||
      parsed_block_size > max_transfer_size ||
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
// (`CBW` -> optional data phase -> `CSW`, with data/CSW `STALL` clearing and
// BOT Reset Recovery) over `device::mojom::UsbDevice`.
class UsbBotTransport {
 public:
  // Outcome of one BOT transaction.
  enum class Result {
    // A transfer or BOT Reset Recovery failed, or the CSW was unusable.
    kTransportError,
    // The CSW reported `kCswStatusCommandPassed`.
    kCommandPassed,
    // The CSW reported `kCswStatusCommandFailed` (`CHECK CONDITION`).
    kCommandFailed,
    // The command should be resent as is: the CSW reported a Phase Error and
    // BOT Reset Recovery completed, or a data-out phase stalled or was only
    // partially accepted even though the CSW reported Command Passed.
    kRetryCommand,
  };
  using TransactionCallback =
      base::OnceCallback<void(Result result,
                              std::vector<uint8_t> received_data)>;

  UsbBotTransport(device::mojom::UsbDevice* device,
                  UsbMassStorageEndpoints endpoints,
                  uint32_t* next_tag)
      : device_(device),
        endpoints_(endpoints),
        next_tag_(CHECK_DEREF(next_tag)) {}

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
    CHECK(!std::holds_alternative<std::monostate>(data_phase));
    tag_ = ++(*next_tag_);
    data_phase_ = data_phase;
    timeout_ms_ = static_cast<uint32_t>(timeout.InMilliseconds());
    expected_data_length_ = 0;
    is_data_out_ = false;
    data_phase_stalled_ = false;
    retried_csw_after_stall_ = false;
    received_data_.clear();
    callback_ = std::move(callback);

    uint8_t direction_flags = kDirectionHostToDevice;
    if (const auto* data_in = std::get_if<DataInPhase>(&data_phase_)) {
      CHECK_GT(data_in->length, 0u);
      expected_data_length_ = data_in->length;
      direction_flags = kDirectionDeviceToHost;
    } else if (const auto* data_out = std::get_if<DataOutPhase>(&data_phase_)) {
      CHECK(!data_out->data.empty());
      expected_data_length_ =
          base::checked_cast<uint32_t>(data_out->data.size());
      is_data_out_ = true;
      direction_flags = kDirectionHostToDevice;
    }

    std::vector<uint8_t> command_block = BuildCommandBlockWrapper(
        tag_, expected_data_length_, direction_flags, scsi_command);
    device_->GenericTransferOut(
        endpoints_.endpoint_out, command_block, timeout_ms_,
        base::BindOnce(&UsbBotTransport::OnCommandBlockSent,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnCommandBlockSent(device::mojom::UsbTransferStatus status) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED) {
      data_phase_ = std::monostate{};
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    if (const auto* data_in = std::get_if<DataInPhase>(&data_phase_)) {
      device_->GenericTransferIn(
          endpoints_.endpoint_in, data_in->length, timeout_ms_,
          base::BindOnce(&UsbBotTransport::OnDataInReceived,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    if (const auto* data_out = std::get_if<DataOutPhase>(&data_phase_)) {
      device_->GenericTransferOut(
          endpoints_.endpoint_out, data_out->data, timeout_ms_,
          base::BindOnce(&UsbBotTransport::OnDataOutSent,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    CHECK(std::holds_alternative<NoDataPhase>(data_phase_));
    ReadCommandStatus();
  }

  void OnDataInReceived(device::mojom::UsbTransferStatus status,
                        base::span<const uint8_t> data) {
    if (status == device::mojom::UsbTransferStatus::STALLED) {
      data_phase_stalled_ = true;
      received_data_.assign(data.begin(), data.end());
      device_->ClearHalt(device::mojom::UsbTransferDirection::INBOUND,
                         endpoints_.endpoint_in,
                         base::BindOnce(&UsbBotTransport::OnDataStallCleared,
                                        weak_factory_.GetWeakPtr()));
      return;
    }
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      data_phase_ = std::monostate{};
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    received_data_.assign(data.begin(), data.end());
    ReadCommandStatus();
  }

  void OnDataOutSent(device::mojom::UsbTransferStatus status) {
    data_phase_ = std::monostate{};
    if (status == device::mojom::UsbTransferStatus::STALLED) {
      data_phase_stalled_ = true;
      device_->ClearHalt(device::mojom::UsbTransferDirection::OUTBOUND,
                         endpoints_.endpoint_out,
                         base::BindOnce(&UsbBotTransport::OnDataStallCleared,
                                        weak_factory_.GetWeakPtr()));
      return;
    }
    if (status != device::mojom::UsbTransferStatus::COMPLETED) {
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    ReadCommandStatus();
  }

  void OnDataStallCleared(bool success) {
    if (!success) {
      data_phase_ = std::monostate{};
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    ReadCommandStatus();
  }

  void ReadCommandStatus() {
    data_phase_ = std::monostate{};
    device_->GenericTransferIn(
        endpoints_.endpoint_in, kCommandStatusWrapperSize, timeout_ms_,
        base::BindOnce(&UsbBotTransport::OnCommandStatusReceived,
                       weak_factory_.GetWeakPtr()));
  }

  void OnCommandStatusReceived(device::mojom::UsbTransferStatus status,
                               base::span<const uint8_t> status_bytes) {
    // Per BOT 1.0 §5.3.3 / Figure 2, clear `INBOUND` halt and retry reading the
    // CSW once on the first `STALLED` status; if the retry also stalls, perform
    // Reset Recovery (§5.3.4).
    if (status == device::mojom::UsbTransferStatus::STALLED) {
      if (!retried_csw_after_stall_) {
        retried_csw_after_stall_ = true;
        device_->ClearHalt(device::mojom::UsbTransferDirection::INBOUND,
                           endpoints_.endpoint_in,
                           base::BindOnce(&UsbBotTransport::OnCswStallCleared,
                                          weak_factory_.GetWeakPtr()));
        return;
      }
      StartResetRecovery(Result::kTransportError);
      return;
    }
    if (status != device::mojom::UsbTransferStatus::COMPLETED &&
        status != device::mojom::UsbTransferStatus::SHORT_PACKET) {
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    std::optional<CommandStatusWrapper> status_wrapper =
        ParseCommandStatusWrapper(status_bytes);
    // Per BOT 1.0 §6.3 & §6.5, perform Reset Recovery when the CSW is invalid,
    // reports a Phase Error (`bCSWStatus == 2`), or is non-meaningful
    // (`bCSWStatus > 2` or `dCSWDataResidue > dCBWDataTransferLength` for
    // status 0/1).
    if (!status_wrapper ||
        status_wrapper->signature != kCommandStatusWrapperSignature ||
        status_wrapper->tag != tag_ ||
        status_wrapper->status > kCswStatusPhaseError) {
      StartResetRecovery(Result::kTransportError);
      return;
    }
    if (status_wrapper->status == kCswStatusPhaseError) {
      StartResetRecovery(Result::kRetryCommand);
      return;
    }
    if (status_wrapper->data_residue > expected_data_length_) {
      StartResetRecovery(Result::kTransportError);
      return;
    }
    if (is_data_out_) {
      // A host-to-device transfer that stalled or left residue but still
      // reports `kCswStatusCommandPassed` (BOT 1.0 §6.7.3 Case 11) may have
      // left sectors unwritten. Never let it pass silently; resending the same
      // `WRITE (10)` is idempotent, so ask the caller to retry it.
      if (status_wrapper->status == kCswStatusCommandPassed &&
          (data_phase_stalled_ || status_wrapper->data_residue != 0)) {
        return std::move(callback_).Run(Result::kRetryCommand, {});
      }
    } else {
      const size_t transferred =
          expected_data_length_ - status_wrapper->data_residue;
      if (received_data_.size() > transferred) {
        received_data_.resize(transferred);
      }
    }
    std::move(callback_).Run(status_wrapper->status == kCswStatusCommandPassed
                                 ? Result::kCommandPassed
                                 : Result::kCommandFailed,
                             std::move(received_data_));
  }

  void OnCswStallCleared(bool success) {
    if (!success) {
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    ReadCommandStatus();
  }

  void StartResetRecovery(Result result_after_reset) {
    received_data_.clear();
    auto params = device::mojom::UsbControlTransferParams::New();
    params->type = device::mojom::UsbControlTransferType::CLASS;
    params->recipient = device::mojom::UsbControlTransferRecipient::INTERFACE;
    params->request = kBotBulkOnlyMassStorageResetRequest;
    params->value = 0;
    params->index = endpoints_.interface_number;
    device_->ControlTransferOut(
        std::move(params), {},
        static_cast<uint32_t>(kUsbCommandTimeout.InMilliseconds()),
        base::BindOnce(&UsbBotTransport::OnResetControlTransferDone,
                       weak_factory_.GetWeakPtr(), result_after_reset));
  }

  void OnResetControlTransferDone(Result result_after_reset,
                                  device::mojom::UsbTransferStatus status) {
    if (status != device::mojom::UsbTransferStatus::COMPLETED) {
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    device_->ClearHalt(
        device::mojom::UsbTransferDirection::INBOUND, endpoints_.endpoint_in,
        base::BindOnce(&UsbBotTransport::OnResetInboundCleared,
                       weak_factory_.GetWeakPtr(), result_after_reset));
  }

  void OnResetInboundCleared(Result result_after_reset, bool success) {
    if (!success) {
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    device_->ClearHalt(
        device::mojom::UsbTransferDirection::OUTBOUND, endpoints_.endpoint_out,
        base::BindOnce(&UsbBotTransport::OnResetOutboundCleared,
                       weak_factory_.GetWeakPtr(), result_after_reset));
  }

  void OnResetOutboundCleared(Result result_after_reset, bool success) {
    if (!success) {
      return std::move(callback_).Run(Result::kTransportError, {});
    }
    std::move(callback_).Run(result_after_reset, {});
  }

  const raw_ptr<device::mojom::UsbDevice> device_;
  const UsbMassStorageEndpoints endpoints_;
  const raw_ref<uint32_t> next_tag_;
  uint32_t tag_ = 0;
  DataPhase data_phase_;
  uint32_t timeout_ms_ = 0;
  uint32_t expected_data_length_ = 0;
  bool is_data_out_ = false;
  bool data_phase_stalled_ = false;
  bool retried_csw_after_stall_ = false;
  std::vector<uint8_t> received_data_;
  TransactionCallback callback_;
  base::WeakPtrFactory<UsbBotTransport> weak_factory_{this};
};

// Executes SCSI commands over `UsbBotTransport`, resending a command when the
// transport reports `kRetryCommand` (after a Phase Error or an incomplete
// data-out phase) and issuing `REQUEST SENSE` on `CHECK CONDITION`
// (`kCswStatusCommandFailed`) to retry transient `UNIT ATTENTION` or
// `NOT READY` (becoming ready) conditions.
class UsbScsiCommandRunner {
 public:
  using CommandCallback =
      base::OnceCallback<void(bool ok, std::vector<uint8_t> data)>;

  UsbScsiCommandRunner(device::mojom::UsbDevice* device,
                       UsbMassStorageEndpoints endpoints,
                       uint32_t* next_tag)
      : transport_(device, endpoints, next_tag) {}

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
    CHECK(!std::holds_alternative<std::monostate>(data_phase));
    scsi_command_ = std::move(scsi_command);
    data_phase_ = data_phase;
    attempts_left_ = attempts_left;
    timeout_ = timeout;
    callback_ = std::move(callback);
    AttemptCommand();
  }

  uint8_t last_sense_key() const { return last_sense_key_; }

 private:
  void AttemptCommand() {
    last_sense_key_ = 0;
    transport_.ExecuteTransaction(
        scsi_command_, data_phase_, timeout_,
        base::BindOnce(&UsbScsiCommandRunner::OnCommandTransactionDone,
                       base::Unretained(this)));
  }

  void OnCommandTransactionDone(UsbBotTransport::Result result,
                                std::vector<uint8_t> received_data) {
    if (result == UsbBotTransport::Result::kCommandPassed) {
      data_phase_ = std::monostate{};
      return std::move(callback_).Run(true, std::move(received_data));
    }
    if (result == UsbBotTransport::Result::kRetryCommand &&
        attempts_left_ > 1) {
      // `UsbBotTransport` has already left the device ready for a new CBW.
      --attempts_left_;
      AttemptCommand();
      return;
    }
    if (result != UsbBotTransport::Result::kCommandFailed) {
      data_phase_ = std::monostate{};
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

  void OnRequestSenseTransactionDone(UsbBotTransport::Result result,
                                     std::vector<uint8_t> sense_data) {
    // USB Mass Storage devices return fixed-format sense data (response code
    // 0x70 for current errors or 0x71 for deferred errors), where the sense key
    // is in byte 2 (bits 0..3) and ASC / ASCQ are in bytes 12 and 13.
    if (result != UsbBotTransport::Result::kCommandPassed ||
        sense_data.size() < 14) {
      data_phase_ = std::monostate{};
      return std::move(callback_).Run(false, {});
    }
    const uint8_t response_code = sense_data[0] & 0x7F;
    if (response_code != 0x70 && response_code != 0x71) {
      data_phase_ = std::monostate{};
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
      data_phase_ = std::monostate{};
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

// Models an open and claimed USB Mass Storage Bulk-Only Transport connection
// over `device::mojom::UsbDevice` on `Operation::task_runner_`. Once `Open()`
// succeeds, the device remains open and its Mass Storage interface remains
// claimed across `Write()` and `VerifyWrite()` until `Close()` (or destruction
// on cancellation).
class MojoUsbDeviceConnection : public Operation::UsbDeviceConnection {
 public:
  using CommandCallback = UsbScsiCommandRunner::CommandCallback;
  using ScopedCommandHandle = base::ScopedClosureRunner;

  MojoUsbDeviceConnection(
      UsbMassStorageEndpoints endpoints,
      mojo::PendingRemote<device::mojom::UsbDevice> device_remote,
      base::RepeatingCallback<bool()> is_cancelled)
      : endpoints_(endpoints),
        max_transfer_size_(endpoints.max_packet_size >= kUsb3BulkMaxPacketSize
                               ? kUsb3TransferBufferSize
                               : kUsb2TransferBufferSize),
        device_(std::move(device_remote)),
        is_cancelled_(std::move(is_cancelled)) {}

  MojoUsbDeviceConnection(const MojoUsbDeviceConnection&) = delete;
  MojoUsbDeviceConnection& operator=(const MojoUsbDeviceConnection&) = delete;
  ~MojoUsbDeviceConnection() override = default;

  // Operation::UsbDeviceConnection:
  void Open(CompletionCallback callback) override;
  void Close(base::OnceClosure callback) override;
  // Defined after `MojoUsbBotTransfer` below.
  std::unique_ptr<Operation::UsbTransfer> CreateWriteTransfer(
      base::FilePath image_path,
      bool wipe_backup_gpt,
      base::RepeatingCallback<bool()> is_cancelled,
      base::RepeatingCallback<void(int)> set_progress) override;
  std::unique_ptr<Operation::UsbTransfer> CreateVerifyTransfer(
      base::FilePath image_path,
      base::RepeatingCallback<bool()> is_cancelled,
      base::RepeatingCallback<void(int)> set_progress) override;

  bool is_open() const { return opened_ && claimed_ && device_.is_connected(); }
  uint32_t block_size() const { return block_size_; }
  uint32_t last_block_index() const { return last_block_index_; }
  size_t max_transfer_size() const { return max_transfer_size_; }
  uint8_t last_sense_key() const { return last_sense_key_; }
  uint64_t device_capacity_bytes() const {
    return (static_cast<uint64_t>(last_block_index_) + 1) *
           static_cast<uint64_t>(block_size_);
  }

  // Sends a SCSI command over this connection, retrying on transient unit
  // attention or not-ready sense conditions. Because USB Mass Storage Bulk-Only
  // Transport is strictly half-duplex (`CBW` -> optional data phase -> `CSW` on
  // the same bulk endpoints), at most one command may be in flight on a
  // connection at a time (`CHECK(!active_command_)`). Destroying the returned
  // `ScopedCommandHandle` before `callback` runs cancels the in-flight command.
  // Any `DataOutPhase::data` span must remain valid until `callback` runs or
  // the returned handle is destroyed.
  [[nodiscard]] ScopedCommandHandle SendCommandWithRetry(
      std::vector<uint8_t> scsi_command,
      DataPhase data_phase,
      int attempts_left,
      base::TimeDelta timeout,
      CommandCallback callback) {
    CHECK(!active_command_);
    last_sense_key_ = 0;
    const uint64_t command_id = ++next_command_id_;
    active_command_id_ = command_id;
    active_command_ = std::make_unique<UsbScsiCommandRunner>(
        device_.get(), endpoints_, &next_tag_);
    active_command_->SendCommandWithRetry(
        std::move(scsi_command), data_phase, attempts_left, timeout,
        base::BindOnce(&MojoUsbDeviceConnection::OnCommandComplete,
                       base::Unretained(this), std::move(callback)));
    return ScopedCommandHandle(
        base::BindOnce(&MojoUsbDeviceConnection::CancelCommand,
                       weak_factory_.GetWeakPtr(), command_id));
  }

  void SetTransferDisconnectCallback(base::OnceClosure callback) {
    disconnect_callback_ = std::move(callback);
  }

 private:
  void OnOpened(device::mojom::UsbOpenDeviceResultPtr result);
  void OnClaimed(device::mojom::UsbClaimInterfaceResult result);
  void OnTestUnitReadyDone(bool ok, std::vector<uint8_t> data);
  void OnReadCapacityDone(bool ok, std::vector<uint8_t> data);
  void FailOpen(const std::string& error);
  void OnReleased(bool success);
  void OnClosed();
  void OnDisconnected();
  void OnCommandComplete(CommandCallback callback,
                         bool ok,
                         std::vector<uint8_t> data);
  void CancelCommand(uint64_t command_id);

  const UsbMassStorageEndpoints endpoints_;
  const size_t max_transfer_size_;
  mojo::Remote<device::mojom::UsbDevice> device_;
  uint32_t next_tag_ = 0;
  uint64_t next_command_id_ = 0;
  uint64_t active_command_id_ = 0;
  uint8_t last_sense_key_ = 0;
  std::unique_ptr<UsbScsiCommandRunner> active_command_;
  ScopedCommandHandle pending_command_;
  base::RepeatingCallback<bool()> is_cancelled_;
  CompletionCallback open_callback_;
  base::OnceClosure disconnect_callback_;
  base::OnceClosure close_callback_;
  uint32_t block_size_ = kDefaultBlockSize;
  // 0-based logical block index of the last block on the device.
  uint32_t last_block_index_ = 0;
  bool opened_ = false;
  bool claimed_ = false;
  base::WeakPtrFactory<MojoUsbDeviceConnection> weak_factory_{this};
};

void MojoUsbDeviceConnection::Open(CompletionCallback callback) {
  CHECK(callback);
  CHECK(!open_callback_);
  if (is_cancelled_.Run()) {
    std::move(callback).Run(base::unexpected(error::kAborted));
    return;
  }
  open_callback_ = std::move(callback);
  device_.set_disconnect_handler(base::BindOnce(
      &MojoUsbDeviceConnection::OnDisconnected, base::Unretained(this)));
  device_->Open(base::BindOnce(&MojoUsbDeviceConnection::OnOpened,
                               base::Unretained(this)));
}

void MojoUsbDeviceConnection::Close(base::OnceClosure callback) {
  CHECK(callback);
  CHECK(!open_callback_);
  CHECK(!close_callback_);
  disconnect_callback_.Reset();
  pending_command_.RunAndReset();
  active_command_.reset();
  active_command_id_ = 0;
  if (!device_.is_connected()) {
    claimed_ = false;
    opened_ = false;
    std::move(callback).Run();
    return;
  }
  close_callback_ = std::move(callback);
  if (claimed_) {
    claimed_ = false;
    device_->ReleaseInterface(
        endpoints_.interface_number,
        base::BindOnce(&MojoUsbDeviceConnection::OnReleased,
                       base::Unretained(this)));
    return;
  }
  OnReleased(true);
}

void MojoUsbDeviceConnection::OnOpened(
    device::mojom::UsbOpenDeviceResultPtr result) {
  if (result && result->is_success()) {
    opened_ = true;
  }
  if (is_cancelled_.Run()) {
    FailOpen(error::kAborted);
    return;
  }
  if (!opened_) {
    FailOpen(error::kDeviceOpenError);
    return;
  }
  device_->ClaimInterface(endpoints_.interface_number,
                          base::BindOnce(&MojoUsbDeviceConnection::OnClaimed,
                                         base::Unretained(this)));
}

void MojoUsbDeviceConnection::OnClaimed(
    device::mojom::UsbClaimInterfaceResult result) {
  if (result == device::mojom::UsbClaimInterfaceResult::kSuccess) {
    claimed_ = true;
  }
  if (is_cancelled_.Run()) {
    FailOpen(error::kAborted);
    return;
  }
  if (!claimed_) {
    FailOpen(error::kDeviceOpenError);
    return;
  }
  // Send SCSI `TEST UNIT READY` (0x00) to clear any initial `UNIT ATTENTION`
  // condition before querying block size and capacity.
  pending_command_ = SendCommandWithRetry(
      {kScsiTestUnitReadyOpcode, 0, 0, 0, 0, 0}, NoDataPhase{},
      /*attempts_left=*/3, kUsbCommandTimeout,
      base::BindOnce(&MojoUsbDeviceConnection::OnTestUnitReadyDone,
                     base::Unretained(this)));
}

void MojoUsbDeviceConnection::OnTestUnitReadyDone(bool ok,
                                                  std::vector<uint8_t> data) {
  pending_command_.RunAndReset();
  if (is_cancelled_.Run()) {
    FailOpen(error::kAborted);
    return;
  }
  if (!ok) {
    FailOpen(error::kDeviceOpenError);
    return;
  }
  pending_command_ = SendCommandWithRetry(
      {kScsiReadCapacity10Opcode, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      DataInPhase{kScsiReadCapacity10ResponseLength},
      /*attempts_left=*/3, kUsbCommandTimeout,
      base::BindOnce(&MojoUsbDeviceConnection::OnReadCapacityDone,
                     base::Unretained(this)));
}

void MojoUsbDeviceConnection::OnReadCapacityDone(bool ok,
                                                 std::vector<uint8_t> data) {
  pending_command_.RunAndReset();
  if (is_cancelled_.Run()) {
    FailOpen(error::kAborted);
    return;
  }
  std::optional<Capacity10Response> capacity =
      ok ? ParseReadCapacity10Response(data, max_transfer_size_) : std::nullopt;
  if (!capacity.has_value()) {
    FailOpen(error::kDeviceOpenError);
    return;
  }
  last_block_index_ = capacity->last_logical_block_address;
  block_size_ = capacity->block_size;
  // `ParseReadCapacity10Response()` rejects `block_size > max_transfer_size`.
  CHECK_LE(block_size_, max_transfer_size_);
  std::move(open_callback_).Run(base::ok());
}

void MojoUsbDeviceConnection::FailOpen(const std::string& error) {
  CHECK(open_callback_);
  Close(base::BindOnce(std::move(open_callback_), base::unexpected(error)));
}

void MojoUsbDeviceConnection::OnReleased(bool /*success*/) {
  if (opened_ && device_.is_connected()) {
    opened_ = false;
    device_->Close(base::BindOnce(&MojoUsbDeviceConnection::OnClosed,
                                  base::Unretained(this)));
    return;
  }
  opened_ = false;
  OnClosed();
}

void MojoUsbDeviceConnection::OnClosed() {
  std::move(close_callback_).Run();
  // `this` has been destroyed by `Operation::OnUsbConnectionClosed`.
}

void MojoUsbDeviceConnection::OnDisconnected() {
  claimed_ = false;
  opened_ = false;
  pending_command_.RunAndReset();
  active_command_.reset();
  active_command_id_ = 0;
  if (open_callback_) {
    std::move(open_callback_).Run(base::unexpected(error::kDeviceOpenError));
    return;
  }
  if (disconnect_callback_) {
    std::move(disconnect_callback_).Run();
    return;
  }
  if (close_callback_) {
    std::move(close_callback_).Run();
    return;
  }
}

void MojoUsbDeviceConnection::OnCommandComplete(CommandCallback callback,
                                                bool ok,
                                                std::vector<uint8_t> data) {
  CHECK(active_command_);
  last_sense_key_ = active_command_->last_sense_key();
  active_command_.reset();
  active_command_id_ = 0;
  std::move(callback).Run(ok, std::move(data));
}

void MojoUsbDeviceConnection::CancelCommand(uint64_t command_id) {
  if (active_command_id_ == command_id) {
    active_command_.reset();
    active_command_id_ = 0;
  }
}

// Streams a single `Write` or `Verify` pass of `image_path_` over an open
// `MojoUsbDeviceConnection` on `Operation::task_runner_`.
class MojoUsbBotTransfer : public Operation::UsbTransfer {
 public:
  MojoUsbBotTransfer(MojoUsbDeviceConnection* connection,
                     base::FilePath image_path,
                     TransferMode mode,
                     bool wipe_backup_gpt,
                     base::RepeatingCallback<bool()> is_cancelled,
                     base::RepeatingCallback<void(int)> set_progress)
      : connection_(CHECK_DEREF(connection)),
        image_path_(std::move(image_path)),
        mode_(mode),
        wipe_backup_gpt_(wipe_backup_gpt),
        is_cancelled_(std::move(is_cancelled)),
        set_progress_(std::move(set_progress)) {}

  MojoUsbBotTransfer(const MojoUsbBotTransfer&) = delete;
  MojoUsbBotTransfer& operator=(const MojoUsbBotTransfer&) = delete;
  ~MojoUsbBotTransfer() override {
    connection_->SetTransferDisconnectCallback(base::NullCallback());
    pending_command_.RunAndReset();
  }

  void Start(CompletionCallback callback) override {
    CHECK(callback);
    CHECK(!callback_);
    callback_ = std::move(callback);
    // Defensive check: `Operation::Cancel()` and `Abort()` run `CleanUp()` on
    // `task_runner_` and destroy `usb_transfer_` synchronously.
    if (is_cancelled_.Run()) {
      return Finish(base::unexpected(error::kAborted));
    }
    if (!connection_->is_open()) {
      return Finish(base::unexpected(error::kDeviceOpenError));
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
    // Return `kDeviceWriteError` (or `kHashReadError` during verification) when
    // the image exceeds the target device's capacity to match the desktop
    // ImageWriter implementation (and what the Chromebook Recovery Utility
    // extension expects during the write stage).
    if (static_cast<uint64_t>(total_size_) >
        connection_->device_capacity_bytes()) {
      return Finish(base::unexpected(mode_ == TransferMode::kWrite
                                         ? error::kDeviceWriteError
                                         : error::kHashReadError));
    }

    const uint32_t block_size = connection_->block_size();
    const size_t max_transfer_size = connection_->max_transfer_size();
    // Cap `chunk_size_` at `total_size_ / 2` so small test images still emit an
    // intermediate progress event before 100%.
    const size_t clamped_half = static_cast<size_t>(
        std::clamp<int64_t>(total_size_ / 2, block_size, max_transfer_size));
    chunk_size_ =
        std::max<size_t>(block_size, (clamped_half / block_size) * block_size);

    connection_->SetTransferDisconnectCallback(base::BindOnce(
        &MojoUsbBotTransfer::OnDisconnected, base::Unretained(this)));
    ProcessNextChunk();
  }

 private:
  void ProcessNextChunk() {
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
      const uint32_t last_block_index = connection_->last_block_index();
      const uint32_t block_size = connection_->block_size();
      if (wipe_backup_gpt_ && !wrote_backup_gpt_ &&
          last_block_index >= primary_blocks_written + kBackupGptBlocks) {
        wrote_backup_gpt_ = true;
        current_chunk_.assign(
            static_cast<size_t>(kBackupGptBlocks) * block_size, 0);
        const uint32_t backup_gpt_start =
            last_block_index - (kBackupGptBlocks - 1);
        SendCommandWithRetry(
            BuildScsiReadWrite10Command(kScsiWrite10Opcode, backup_gpt_start,
                                        kBackupGptBlocks),
            DataOutPhase{current_chunk_}, /*attempts_left=*/3,
            kUsbCommandTimeout,
            base::BindOnce(&MojoUsbBotTransfer::OnBackupGptWiped,
                           base::Unretained(this)));
        return;
      }
      SendCommandWithRetry(
          {kScsiSynchronizeCache10Opcode, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          NoDataPhase{}, /*attempts_left=*/3, kUsbSyncCacheTimeout,
          base::BindOnce(&MojoUsbBotTransfer::OnSynchronizeCacheDone,
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

    const uint32_t block_size = connection_->block_size();
    const size_t aligned_size =
        AlignAndZeroPad(&current_chunk_, *bytes_read, block_size);
    const uint16_t num_blocks =
        static_cast<uint16_t>(aligned_size / block_size);
    const bool is_write = (mode_ == TransferMode::kWrite);
    SendCommandWithRetry(
        BuildScsiReadWrite10Command(
            is_write ? kScsiWrite10Opcode : kScsiRead10Opcode,
            current_block_index_, num_blocks),
        is_write ? DataPhase(DataOutPhase{current_chunk_})
                 : DataPhase(DataInPhase{static_cast<uint32_t>(aligned_size)}),
        /*attempts_left=*/3, kUsbCommandTimeout,
        base::BindOnce(&MojoUsbBotTransfer::OnChunkDone, base::Unretained(this),
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
    if (!ok && connection_->last_sense_key() != kScsiSenseKeyIllegalRequest) {
      return Finish(base::unexpected(error::kDeviceWriteError));
    }
    Finish(base::ok());
  }

  void SendCommandWithRetry(std::vector<uint8_t> scsi_command,
                            DataPhase data_phase,
                            int attempts_left,
                            base::TimeDelta timeout,
                            MojoUsbDeviceConnection::CommandCallback callback) {
    if (is_cancelled_.Run()) {
      return Finish(base::unexpected(error::kAborted));
    }
    pending_command_ = connection_->SendCommandWithRetry(
        std::move(scsi_command), data_phase, attempts_left, timeout,
        std::move(callback));
  }

  void OnDisconnected() { Finish(base::unexpected(error::kDeviceOpenError)); }

  // Invokes `callback_` (`Operation::OnUsbTransferFinished`), which resets
  // `Operation::usb_transfer_` and destroys `this`; callers must be in tail
  // position and must not touch any members afterwards.
  void Finish(base::expected<void, std::string> result) {
    CHECK(callback_);
    connection_->SetTransferDisconnectCallback(base::NullCallback());
    pending_command_.RunAndReset();
    image_file_.Close();
    std::move(callback_).Run(std::move(result));
    // `this` has been destroyed.
  }

  const raw_ref<MojoUsbDeviceConnection> connection_;
  const base::FilePath image_path_;
  const TransferMode mode_;
  const bool wipe_backup_gpt_;
  std::vector<uint8_t> current_chunk_;
  // Must be declared after `current_chunk_` so it is destroyed before
  // `current_chunk_`: an in-flight `WRITE (10)` command holds a `DataOutPhase`
  // `raw_span` pointing into `current_chunk_`.
  MojoUsbDeviceConnection::ScopedCommandHandle pending_command_;
  base::RepeatingCallback<bool()> is_cancelled_;
  base::RepeatingCallback<void(int)> set_progress_;
  CompletionCallback callback_;
  base::File image_file_;
  int64_t total_size_ = 0;
  int64_t bytes_processed_ = 0;
  // 0-based logical block index in units of `connection_->block_size()`-byte
  // logical blocks.
  uint32_t current_block_index_ = 0;
  size_t chunk_size_ = kUsb3TransferBufferSize;
  bool wrote_backup_gpt_ = false;
};

std::unique_ptr<Operation::UsbTransfer>
MojoUsbDeviceConnection::CreateWriteTransfer(
    base::FilePath image_path,
    bool wipe_backup_gpt,
    base::RepeatingCallback<bool()> is_cancelled,
    base::RepeatingCallback<void(int)> set_progress) {
  return std::make_unique<MojoUsbBotTransfer>(
      this, std::move(image_path), TransferMode::kWrite, wipe_backup_gpt,
      std::move(is_cancelled), std::move(set_progress));
}

std::unique_ptr<Operation::UsbTransfer>
MojoUsbDeviceConnection::CreateVerifyTransfer(
    base::FilePath image_path,
    base::RepeatingCallback<bool()> is_cancelled,
    base::RepeatingCallback<void(int)> set_progress) {
  return std::make_unique<MojoUsbBotTransfer>(
      this, std::move(image_path), TransferMode::kVerify,
      /*wipe_backup_gpt=*/false, std::move(is_cancelled),
      std::move(set_progress));
}

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
// run on `Operation::task_runner_`, each chunk is posted back onto
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
// up to `kUsb3TransferBufferSize` posted onto `Operation::task_runner_`.
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
      std::clamp<int64_t>(total_size / 2, 1, kUsb3TransferBufferSize));
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

void Operation::EnsureUsbConnection(ParsedDeviceId device_id,
                                    base::OnceClosure on_connected) {
  CHECK(IsRunningInCorrectSequence());
  CHECK(!usb_manager_);
  if (usb_connection_) {
    std::move(on_connected).Run();
    return;
  }
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
                               std::move(on_connected)));
}

void Operation::OnUsbManagerDisconnected() {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    CloseUsbConnection(base::DoNothing());
    return;
  }
  CloseUsbConnection(
      base::BindOnce(&Operation::Error, this, error::kDeviceOpenError));
}

void Operation::OnUsbDeviceInfoRefreshed(ParsedDeviceId device_id,
                                         base::OnceClosure on_connected,
                                         device::mojom::UsbDeviceInfoPtr info) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    CloseUsbConnection(base::DoNothing());
    return;
  }
  if (!info || info->vendor_id != device_id.vendor_id ||
      info->product_id != device_id.product_id) {
    CloseUsbConnection(
        base::BindOnce(&Operation::Error, this, error::kDeviceOpenError));
    return;
  }
  std::optional<UsbMassStorageEndpoints> endpoints =
      ExtractMassStorageEndpoints(*info);
  if (!endpoints.has_value()) {
    CloseUsbConnection(
        base::BindOnce(&Operation::Error, this, error::kDeviceOpenError));
    return;
  }
  mojo::PendingRemote<device::mojom::UsbDevice> device_remote;
  (*usb_manager_)
      ->GetUnrestrictedDevice(device_id.guid, {},
                              device_remote.InitWithNewPipeAndPassReceiver(),
                              mojo::NullRemote());
  // Safe: Mojo allows destroying the Remote from within its response callback.
  usb_manager_.reset();
  if (!device_remote.is_valid()) {
    CloseUsbConnection(
        base::BindOnce(&Operation::Error, this, error::kDeviceOpenError));
    return;
  }
  CHECK(!usb_connection_);
  auto connection = std::make_unique<MojoUsbDeviceConnection>(
      *endpoints, std::move(device_remote),
      base::BindRepeating(&Operation::IsCancelled, base::Unretained(this)));
  UsbDeviceConnection* raw_connection = connection.get();
  raw_connection->Open(base::BindOnce(&Operation::OnUsbConnectionOpened, this,
                                      std::move(connection),
                                      std::move(on_connected)));
}

void Operation::OnUsbConnectionOpened(
    std::unique_ptr<UsbDeviceConnection> connection,
    base::OnceClosure on_connected,
    base::expected<void, std::string> result) {
  CHECK(IsRunningInCorrectSequence());
  if (!result.has_value()) {
    connection.reset();
    if (IsCancelled()) {
      CloseUsbConnection(base::DoNothing());
      return;
    }
    CloseUsbConnection(base::BindOnce(&Operation::Error, this, result.error()));
    return;
  }
  CHECK(!usb_connection_);
  usb_connection_ = std::move(connection);
  if (IsCancelled()) {
    CloseUsbConnection(base::DoNothing());
    return;
  }
  std::move(on_connected).Run();
}

void Operation::StartUsbWrite(base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    CloseUsbConnection(base::DoNothing());
    return;
  }
  CHECK(usb_connection_);
  CHECK(!usb_transfer_);
  usb_transfer_ = usb_connection_->CreateWriteTransfer(
      image_path_, ShouldWipeBackupGpt(),
      base::BindRepeating(&Operation::IsCancelled, base::Unretained(this)),
      base::BindRepeating(&Operation::SetProgress, base::Unretained(this)));
  usb_transfer_->Start(base::BindOnce(&Operation::OnUsbTransferFinished,
                                      base::Unretained(this),
                                      std::move(continuation)));
}

void Operation::StartUsbVerify(base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    CloseUsbConnection(base::DoNothing());
    return;
  }
  CHECK(usb_connection_);
  CHECK(!usb_transfer_);
  usb_transfer_ = usb_connection_->CreateVerifyTransfer(
      image_path_,
      base::BindRepeating(&Operation::IsCancelled, base::Unretained(this)),
      base::BindRepeating(&Operation::SetProgress, base::Unretained(this)));
  usb_transfer_->Start(base::BindOnce(&Operation::OnUsbTransferFinished,
                                      base::Unretained(this),
                                      std::move(continuation)));
}

void Operation::OnUsbTransferFinished(
    base::OnceClosure continuation,
    base::expected<void, std::string> result) {
  CHECK(IsRunningInCorrectSequence());
  usb_transfer_.reset();
  if (IsCancelled()) {
    CloseUsbConnection(base::DoNothing());
    return;
  }
  if (!result.has_value()) {
    CloseUsbConnection(base::BindOnce(&Operation::Error, this, result.error()));
    return;
  }
  CompleteAndContinue(std::move(continuation));
}

void Operation::CloseUsbConnection(base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  usb_transfer_.reset();
  usb_manager_.reset();
  if (!usb_connection_) {
    std::move(continuation).Run();
    return;
  }
  std::unique_ptr<UsbDeviceConnection> connection = std::move(usb_connection_);
  UsbDeviceConnection* raw_connection = connection.get();
  raw_connection->Close(base::BindOnce(&Operation::OnUsbConnectionClosed, this,
                                       std::move(connection),
                                       std::move(continuation)));
}

void Operation::OnUsbConnectionClosed(
    std::unique_ptr<UsbDeviceConnection> connection,
    base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  connection.reset();
  if (IsCancelled()) {
    return;
  }
  std::move(continuation).Run();
}

void Operation::Write(base::OnceClosure continuation) {
  CHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    CloseUsbConnection(base::DoNothing());
    return;
  }
  SetStage(image_writer_api::Stage::kWrite);

  if (std::optional<ParsedDeviceId> parsed_id =
          ParseUsbDeviceId(device_path_.value())) {
    EnsureUsbConnection(
        std::move(*parsed_id),
        base::BindOnce(&Operation::StartUsbWrite, base::Unretained(this),
                       std::move(continuation)));
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
    CloseUsbConnection(base::DoNothing());
    return;
  }
  SetStage(image_writer_api::Stage::kVerifyWrite);

  if (std::optional<ParsedDeviceId> parsed_id =
          ParseUsbDeviceId(device_path_.value())) {
    EnsureUsbConnection(
        std::move(*parsed_id),
        base::BindOnce(&Operation::StartUsbVerify, base::Unretained(this),
                       std::move(continuation)));
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
