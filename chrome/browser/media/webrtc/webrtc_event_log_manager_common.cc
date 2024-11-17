// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

#include <limits>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/zlib/zlib.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#endif

namespace webrtc_event_logging {

using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;

const size_t kWebRtcEventLogManagerUnlimitedFileSize = 0;

const size_t kWebRtcEventLogIdLength = 32;

// Be careful not to change these without updating the number of characters
// reserved in the filename. See kWebAppIdLength.
const size_t kMinWebRtcEventLogWebAppId = 1;
const size_t kMaxWebRtcEventLogWebAppId = 99;

// Sentinel value for an invalid web-app ID.
const size_t kInvalidWebRtcEventLogWebAppId = 0;
static_assert(kInvalidWebRtcEventLogWebAppId < kMinWebRtcEventLogWebAppId ||
                  kInvalidWebRtcEventLogWebAppId > kMaxWebRtcEventLogWebAppId,
              "Sentinel value must be distinct from legal values.");

const char kRemoteBoundWebRtcEventLogFileNamePrefix[] = "webrtc_event_log";

// Important! These values may be relied on by web-apps. Do not change.
const char kStartRemoteLoggingFailureAlreadyLogging[] = "Already logging.";
const char kStartRemoteLoggingFailureDeadRenderProcessHost[] =
    "RPH already dead.";
const char kStartRemoteLoggingFailureFeatureDisabled[] = "Feature disabled.";
const char kStartRemoteLoggingFailureFileCreationError[] =
    "Could not create file.";
const char kStartRemoteLoggingFailureFilePathUsedHistory[] =
    "Used history file path.";
const char kStartRemoteLoggingFailureFilePathUsedLog[] = "Used log file path.";
const char kStartRemoteLoggingFailureIllegalWebAppId[] = "Illegal web-app ID.";
const char kStartRemoteLoggingFailureLoggingDisabledBrowserContext[] =
    "Disabled for browser context.";
const char kStartRemoteLoggingFailureMaxSizeTooLarge[] =
    "Excessively large max log size.";
const char kStartRemoteLoggingFailureMaxSizeTooSmall[] = "Max size too small.";
const char kStartRemoteLoggingFailureNoAdditionalActiveLogsAllowed[] =
    "No additional active logs allowed.";
const char kStartRemoteLoggingFailureOutputPeriodMsTooLarge[] =
    "Excessively large output period (ms).";
const char kStartRemoteLoggingFailureUnknownOrInactivePeerConnection[] =
    "Unknown or inactive peer connection.";
const char kStartRemoteLoggingFailureUnlimitedSizeDisallowed[] =
    "Unlimited size disallowed.";

const BrowserContextId kNullBrowserContextId =
    reinterpret_cast<BrowserContextId>(nullptr);

void UmaRecordWebRtcEventLoggingApi(WebRtcEventLoggingApiUma result) {
  base::UmaHistogramEnumeration("WebRtcEventLogging.Api", result);
}

void UmaRecordWebRtcEventLoggingUpload(WebRtcEventLoggingUploadUma result) {
  base::UmaHistogramEnumeration("WebRtcEventLogging.Upload", result);
}

void UmaRecordWebRtcEventLoggingNetErrorType(int net_error) {
  base::UmaHistogramSparse("WebRtcEventLogging.NetError", net_error);
}

namespace {

constexpr int kDefaultMemLevel = 8;

constexpr size_t kGzipHeaderBytes = 15;
constexpr size_t kGzipFooterBytes = 10;

constexpr size_t kWebAppIdLength = 2;

// Tracks budget over a resource (such as bytes allowed in a file, etc.).
// Allows an unlimited budget.
class Budget {
 public:
  // If !max.has_value(), the budget is unlimited.
  explicit Budget(std::optional<size_t> max) : max_(max), current_(0) {}

  // Check whether the budget allows consuming an additional |consumed| of
  // the resource.
  bool ConsumeAllowed(size_t consumed) const {
    if (!max_.has_value()) {
      return true;
    }

    DCHECK_LE(current_, max_.value());

    const size_t after_consumption = current_ + consumed;

    if (after_consumption < current_) {
      return false;  // Wrap-around.
    } else if (after_consumption > max_.value()) {
      return false;  // Budget exceeded.
    } else {
      return true;
    }
  }

  // Checks whether the budget has been completely used up.
  bool Exhausted() const { return !ConsumeAllowed(0); }

  // Consume an additional |consumed| of the resource.
  void Consume(size_t consumed) {
    DCHECK(ConsumeAllowed(consumed));
    current_ += consumed;
  }

 private:
  const std::optional<size_t> max_;
  size_t current_;
};

// Writes a log to a file while observing a maximum size.
class BaseLogFileWriter : public LogFileWriter {
 public:
  // If !max_file_size_bytes.has_value(), an unlimited writer is created.
  // If it has a value, it must be at least MinFileSizeBytes().
  BaseLogFileWriter(const base::FilePath& path,
                    std::optional<size_t> max_file_size_bytes);

  ~BaseLogFileWriter() override;

  bool Init() override;

  const base::FilePath& path() const override;

  bool MaxSizeReached() const override;

  bool Write(const std::string& input) override;

  bool Close() override;

  void Delete() override;

 protected:
  // * Logs are created PRE_INIT.
  // * If Init() is successful (potentially writing some header to the log),
  //   the log becomes ACTIVE.
  // * Any error puts the log into an unrecoverable ERRORED state. When an
  //   errored file is Close()-ed, it is deleted.
  // * If Write() is ever denied because of budget constraintss, the file
  //   becomes FULL. Only metadata is then allowed (subject to its own budget).
  // * Closing an ACTIVE or FULL file puts it into CLOSED, at which point the
  //   file may be used. (Note that closing itself might also yield an error,
  //   which would put the file into ERRORED, then deleted.)
  // * Closed files may be DELETED.
  enum class State { PRE_INIT, ACTIVE, FULL, CLOSED, ERRORED, DELETED };

  // Setter/getter for |state_|.
  void SetState(State state);
  State state() const { return state_; }

  // Checks whether the budget allows writing an additional |bytes|.
  bool WithinBudget(size_t bytes) const;

  // Writes |input| to the file.
  // May only be called on ACTIVE or FULL files (for FULL files, only metadata
  // such as compression footers, etc., may be written; the budget must still
  // be respected).
  // It's up to the caller to respect the budget; this will DCHECK on it.
  // Returns |true| if writing was successful. |false| indicates an
  // unrecoverable error; the file must be discarded.
  bool WriteInternal(const std::string& input, bool metadata);

  // Finalizes the file (writes metadata such as compression footer, if any).
  // Reports whether the file was successfully finalized. Those which weren't
  // should be discarded.
  virtual bool Finalize();

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::FilePath path_;
  base::File file_;  // Populated by Init().
  State state_;
  Budget budget_;
};

BaseLogFileWriter::BaseLogFileWriter(const base::FilePath& path,
                                     std::optional<size_t> max_file_size_bytes)
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      path_(path),
      state_(State::PRE_INIT),
      budget_(max_file_size_bytes) {}

BaseLogFileWriter::~BaseLogFileWriter() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Chrome shut-down. The original task_runner_ is no longer running, so
    // no risk of concurrent access or races.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }

  if (state() != State::CLOSED && state() != State::DELETED) {
    Close();
  }
}

bool BaseLogFileWriter::Init() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state(), State::PRE_INIT);

  // TODO(crbug.com/40545136): Use a temporary filename which will indicate
  // incompletion, and rename to something that is eligible for upload only
  // on an orderly and successful Close().

  // Attempt to create the file.
  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  file_.Initialize(path_, file_flags);
  if (!file_.IsValid() || !file_.created()) {
    LOG(WARNING) << "Couldn't create remote-bound WebRTC event log file.";
    if (!base::DeleteFile(path_)) {
      LOG(ERROR) << "Failed to delete " << path_ << ".";
    }
    SetState(State::ERRORED);
    return false;
  }

  SetState(State::ACTIVE);

  return true;
}

const base::FilePath& BaseLogFileWriter::path() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return path_;
}

bool BaseLogFileWriter::MaxSizeReached() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state(), State::ACTIVE);
  return !WithinBudget(1);
}

bool BaseLogFileWriter::Write(const std::string& input) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state(), State::ACTIVE);
  DCHECK(!MaxSizeReached());

  if (input.empty()) {
    return true;
  }

  if (!WithinBudget(input.length())) {
    SetState(State::FULL);
    return false;
  }

  const bool did_write = WriteInternal(input, /*metadata=*/false);
  if (!did_write) {
    SetState(State::ERRORED);
  }
  return did_write;
}

bool BaseLogFileWriter::Close() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(state(), State::CLOSED);
  DCHECK_NE(state(), State::DELETED);

  const bool result = ((state() != State::ERRORED) && Finalize());

  if (result) {
    file_.Flush();
    file_.Close();
    SetState(State::CLOSED);
  } else {
    Delete();  // Changes the state to DELETED.
  }

  return result;
}

void BaseLogFileWriter::Delete() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(state(), State::DELETED);

  // The file should be closed before deletion. However, we do not want to go
  // through Finalize() and any potential production of a compression footer,
  // etc., since we'll be discarding the file anyway.
  if (state() != State::CLOSED) {
    file_.Close();
  }

  if (!base::DeleteFile(path_)) {
    LOG(ERROR) << "Failed to delete " << path_ << ".";
  }

  SetState(State::DELETED);
}

void BaseLogFileWriter::SetState(State state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_ = state;
}

bool BaseLogFileWriter::WithinBudget(size_t bytes) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return budget_.ConsumeAllowed(bytes);
}

bool BaseLogFileWriter::WriteInternal(const std::string& input, bool metadata) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state() == State::ACTIVE || (state() == State::FULL && metadata));
  DCHECK(WithinBudget(input.length()));

  // base::File's interface does not allow writing more than
  // numeric_limits<int>::max() bytes at a time.
  DCHECK_LE(input.length(),
            static_cast<size_t>(std::numeric_limits<int>::max()));

  if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(input))) {
    LOG(WARNING) << "WebRTC event log couldn't be written to the "
                    "locally stored file in its entirety.";
    return false;
  }

  budget_.Consume(input.length());
  return true;
}

bool BaseLogFileWriter::Finalize() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(state(), State::CLOSED);
  DCHECK_NE(state(), State::DELETED);
  DCHECK_NE(state(), State::ERRORED);
  return true;
}

// Writes a GZIP-compressed log to a file while observing a maximum size.
class GzippedLogFileWriter : public BaseLogFileWriter {
 public:
  GzippedLogFileWriter(const base::FilePath& path,
                       std::optional<size_t> max_file_size_bytes,
                       std::unique_ptr<LogCompressor> compressor);

  ~GzippedLogFileWriter() override = default;

  bool Init() override;

  bool MaxSizeReached() const override;

  bool Write(const std::string& input) override;

 protected:
  bool Finalize() override;

 private:
  std::unique_ptr<LogCompressor> compressor_;
};

GzippedLogFileWriter::GzippedLogFileWriter(
    const base::FilePath& path,
    std::optional<size_t> max_file_size_bytes,
    std::unique_ptr<LogCompressor> compressor)
    : BaseLogFileWriter(path, max_file_size_bytes),
      compressor_(std::move(compressor)) {
  // Factory validates size before instantiation.
  DCHECK(!max_file_size_bytes.has_value() ||
         max_file_size_bytes.value() >= kGzipOverheadBytes);
}

bool GzippedLogFileWriter::Init() {
  if (!BaseLogFileWriter::Init()) {
    // Super-class should SetState on its own.
    return false;
  }

  std::string header;
  compressor_->CreateHeader(&header);

  const bool result = WriteInternal(header, /*metadata=*/true);
  if (!result) {
    SetState(State::ERRORED);
  }

  return result;
}

bool GzippedLogFileWriter::MaxSizeReached() const {
  DCHECK_EQ(state(), State::ACTIVE);

  // Note that the overhead used (footer only) assumes state() is State::ACTIVE,
  // as DCHECKed above.
  return !WithinBudget(1 + kGzipFooterBytes);
}

bool GzippedLogFileWriter::Write(const std::string& input) {
  DCHECK_EQ(state(), State::ACTIVE);
  DCHECK(!MaxSizeReached());

  if (input.empty()) {
    return true;
  }

  std::string compressed_input;
  const auto result = compressor_->Compress(input, &compressed_input);

  switch (result) {
    case LogCompressor::Result::OK: {
      // |compressor_| guarantees |compressed_input| is within-budget.
      bool did_write = WriteInternal(compressed_input, /*metadata=*/false);
      if (!did_write) {
        SetState(State::ERRORED);
      }
      return did_write;
    }
    case LogCompressor::Result::DISALLOWED: {
      SetState(State::FULL);
      return false;
    }
    case LogCompressor::Result::ERROR_ENCOUNTERED: {
      SetState(State::ERRORED);
      return false;
    }
  }

  NOTREACHED();
}

bool GzippedLogFileWriter::Finalize() {
  DCHECK_NE(state(), State::CLOSED);
  DCHECK_NE(state(), State::DELETED);
  DCHECK_NE(state(), State::ERRORED);

  std::string footer;
  if (!compressor_->CreateFooter(&footer)) {
    LOG(WARNING) << "Compression footer could not be produced.";
    SetState(State::ERRORED);
    return false;
  }

  // |compressor_| guarantees |footer| is within-budget.
  if (!WriteInternal(footer, /*metadata=*/true)) {
    LOG(WARNING) << "Footer could not be written.";
    SetState(State::ERRORED);
    return false;
  }

  return true;
}

// Concrete implementation of LogCompressor using GZIP.
class GzipLogCompressor : public LogCompressor {
 public:
  GzipLogCompressor(
      std::optional<size_t> max_size_bytes,
      std::unique_ptr<CompressedSizeEstimator> compressed_size_estimator);

  ~GzipLogCompressor() override;

  void CreateHeader(std::string* output) override;

  Result Compress(const std::string& input, std::string* output) override;

  bool CreateFooter(std::string* output) override;

 private:
  // * A compressed log starts out empty (PRE_HEADER).
  // * Once the header is produced, the stream is ACTIVE.
  // * If it is ever detected that compressing the next input would exceed the
  //   budget, that input is NOT compressed, and the state becomes FULL, from
  //   which only writing the footer or discarding the file are allowed.
  // * Writing the footer is allowed on an ACTIVE or FULL stream. Then, the
  //   stream is effectively closed.
  // * Any error puts the stream into ERRORED. An errored stream can only
  //   be discarded.
  enum class State { PRE_HEADER, ACTIVE, FULL, POST_FOOTER, ERRORED };

  // Returns the budget left after reserving the GZIP overhead.
  // Optionals without a value, both in the parameters as well as in the
  // return value of the function, signal an unlimited amount.
  static std::optional<size_t> SizeAfterOverheadReservation(
      std::optional<size_t> max_size_bytes);

  // Compresses |input| into |output|, while observing the budget (unless
  // !budgeted). If |last|, also closes the stream.
  Result CompressInternal(const std::string& input,
                          std::string* output,
                          bool budgeted,
                          bool last);

  // Compresses the input data already in |stream_| into |output|.
  bool Deflate(int flush, std::string* output);

  State state_;
  Budget budget_;
  std::unique_ptr<CompressedSizeEstimator> compressed_size_estimator_;
  z_stream stream_;
};

GzipLogCompressor::GzipLogCompressor(
    std::optional<size_t> max_size_bytes,
    std::unique_ptr<CompressedSizeEstimator> compressed_size_estimator)
    : state_(State::PRE_HEADER),
      budget_(SizeAfterOverheadReservation(max_size_bytes)),
      compressed_size_estimator_(std::move(compressed_size_estimator)) {
  memset(&stream_, 0, sizeof(z_stream));
  // Using (MAX_WBITS + 16) triggers the creation of a GZIP header.
  const int result =
      deflateInit2(&stream_, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16,
                   kDefaultMemLevel, Z_DEFAULT_STRATEGY);
  DCHECK_EQ(result, Z_OK);
}

GzipLogCompressor::~GzipLogCompressor() {
  const int result = deflateEnd(&stream_);
  // Z_DATA_ERROR reports that the stream was not properly terminated,
  // but nevertheless correctly released. That happens when we don't
  // write the footer.
  DCHECK(result == Z_OK ||
         (result == Z_DATA_ERROR && state_ != State::POST_FOOTER));
}

void GzipLogCompressor::CreateHeader(std::string* output) {
  DCHECK(output);
  DCHECK(output->empty());
  DCHECK_EQ(state_, State::PRE_HEADER);

  const Result result = CompressInternal(std::string(), output,
                                         /*budgeted=*/false, /*last=*/false);
  DCHECK_EQ(result, Result::OK);
  DCHECK_EQ(output->size(), kGzipHeaderBytes);

  state_ = State::ACTIVE;
}

LogCompressor::Result GzipLogCompressor::Compress(const std::string& input,
                                                  std::string* output) {
  DCHECK_EQ(state_, State::ACTIVE);

  if (input.empty()) {
    return Result::OK;
  }

  const auto result =
      CompressInternal(input, output, /*budgeted=*/true, /*last=*/false);

  switch (result) {
    case Result::OK:
      return result;
    case Result::DISALLOWED:
      state_ = State::FULL;
      return result;
    case Result::ERROR_ENCOUNTERED:
      state_ = State::ERRORED;
      return result;
  }

  NOTREACHED();
}

bool GzipLogCompressor::CreateFooter(std::string* output) {
  DCHECK(output);
  DCHECK(output->empty());
  DCHECK(state_ == State::ACTIVE || state_ == State::FULL);

  const Result result = CompressInternal(std::string(), output,
                                         /*budgeted=*/false, /*last=*/true);
  if (result != Result::OK) {  // !budgeted -> Result::DISALLOWED impossible.
    DCHECK_EQ(result, Result::ERROR_ENCOUNTERED);
    // An error message was logged by CompressInternal().
    state_ = State::ERRORED;
    return false;
  }

  if (output->length() != kGzipFooterBytes) {
    LOG(ERROR) << "Incorrect footer size (" << output->length() << ").";
    state_ = State::ERRORED;
    return false;
  }

  state_ = State::POST_FOOTER;

  return true;
}

std::optional<size_t> GzipLogCompressor::SizeAfterOverheadReservation(
    std::optional<size_t> max_size_bytes) {
  if (!max_size_bytes.has_value()) {
    return std::optional<size_t>();
  } else {
    DCHECK_GE(max_size_bytes.value(), kGzipHeaderBytes + kGzipFooterBytes);
    return max_size_bytes.value() - (kGzipHeaderBytes + kGzipFooterBytes);
  }
}

LogCompressor::Result GzipLogCompressor::CompressInternal(
    const std::string& input,
    std::string* output,
    bool budgeted,
    bool last) {
  DCHECK(output);
  DCHECK(output->empty());
  DCHECK(state_ == State::PRE_HEADER || state_ == State::ACTIVE ||
         (!budgeted && state_ == State::FULL));

  // Avoid writing to |output| unless the return value is OK.
  std::string temp_output;

  if (budgeted) {
    const size_t estimated_compressed_size =
        compressed_size_estimator_->EstimateCompressedSize(input);
    if (!budget_.ConsumeAllowed(estimated_compressed_size)) {
      return Result::DISALLOWED;
    }
  }

  if (last) {
    DCHECK(input.empty());
    stream_.next_in = nullptr;
  } else {
    stream_.next_in = reinterpret_cast<z_const Bytef*>(input.c_str());
  }

  DCHECK_LE(input.length(),
            static_cast<size_t>(std::numeric_limits<uInt>::max()));
  stream_.avail_in = static_cast<uInt>(input.length());

  const bool result = Deflate(last ? Z_FINISH : Z_SYNC_FLUSH, &temp_output);

  stream_.next_in = nullptr;  // Avoid dangling pointers.

  if (!result) {
    // An error message was logged by Deflate().
    return Result::ERROR_ENCOUNTERED;
  }

  if (budgeted) {
    if (!budget_.ConsumeAllowed(temp_output.length())) {
      LOG(WARNING) << "Compressed size was above estimate and unexpectedly "
                      "exceeded the budget.";
      return Result::ERROR_ENCOUNTERED;
    }
    budget_.Consume(temp_output.length());
  }

  std::swap(*output, temp_output);
  return Result::OK;
}

bool GzipLogCompressor::Deflate(int flush, std::string* output) {
  DCHECK((flush != Z_FINISH && stream_.next_in != nullptr) ||
         (flush == Z_FINISH && stream_.next_in == nullptr));
  DCHECK(output->empty());

  bool success = true;  // Result of this method.
  int z_result;         // Result of the zlib function.

  size_t total_compressed_size = 0;

  do {
    // Allocate some additional buffer.
    constexpr uInt kCompressionBuffer = 4 * 1024;
    output->resize(total_compressed_size + kCompressionBuffer);

    // This iteration should write directly beyond previous iterations' last
    // written byte.
    stream_.next_out =
        reinterpret_cast<uint8_t*>(&((*output)[total_compressed_size]));
    stream_.avail_out = kCompressionBuffer;

    z_result = deflate(&stream_, flush);

    DCHECK_GE(kCompressionBuffer, stream_.avail_out);
    const size_t compressed_size = kCompressionBuffer - stream_.avail_out;

    if (flush != Z_FINISH) {
      if (z_result != Z_OK) {
        LOG(ERROR) << "Compression failed (" << z_result << ").";
        success = false;
        break;
      }
    } else {  // flush == Z_FINISH
      // End of the stream; we expect the footer to be exactly the size which
      // we've set aside for it.
      if (z_result != Z_STREAM_END || compressed_size != kGzipFooterBytes) {
        LOG(ERROR) << "Compression failed (" << z_result << ", "
                   << compressed_size << ").";
        success = false;
        break;
      }
    }

    total_compressed_size += compressed_size;
  } while (stream_.avail_out == 0 && z_result != Z_STREAM_END);

  stream_.next_out = nullptr;  // Avoid dangling pointers.

  if (success) {
    output->resize(total_compressed_size);
  } else {
    output->clear();
  }

  return success;
}

// Given a string with a textual representation of a web-app ID, return the
// ID in integer form. If the textual representation does not name a valid
// web-app ID, return kInvalidWebRtcEventLogWebAppId.
size_t ExtractWebAppId(std::string_view str) {
  DCHECK_EQ(str.length(), kWebAppIdLength);

  // Avoid leading '+', etc.
  if (!base::ranges::all_of(str, absl::ascii_isdigit)) {
    return kInvalidWebRtcEventLogWebAppId;
  }

  size_t result;
  if (!base::StringToSizeT(str, &result) ||
      result < kMinWebRtcEventLogWebAppId ||
      result > kMaxWebRtcEventLogWebAppId) {
    return kInvalidWebRtcEventLogWebAppId;
  }
  return result;
}

}  // namespace

const size_t kGzipOverheadBytes = kGzipHeaderBytes + kGzipFooterBytes;

const base::FilePath::CharType kWebRtcEventLogUncompressedExtension[] =
    FILE_PATH_LITERAL("log");
const base::FilePath::CharType kWebRtcEventLogGzippedExtension[] =
    FILE_PATH_LITERAL("log.gz");
const base::FilePath::CharType kWebRtcEventLogHistoryExtension[] =
    FILE_PATH_LITERAL("hist");

size_t BaseLogFileWriterFactory::MinFileSizeBytes() const {
  // No overhead incurred; data written straight to the file without metadata.
  return 0;
}

base::FilePath::StringPieceType BaseLogFileWriterFactory::Extension() const {
  return kWebRtcEventLogUncompressedExtension;
}

std::unique_ptr<LogFileWriter> BaseLogFileWriterFactory::Create(
    const base::FilePath& path,
    std::optional<size_t> max_file_size_bytes) const {
  if (max_file_size_bytes.has_value() &&
      max_file_size_bytes.value() < MinFileSizeBytes()) {
    LOG(WARNING) << "Max size (" << max_file_size_bytes.value()
                 << ") below minimum size (" << MinFileSizeBytes() << ").";
    return nullptr;
  }

  auto result = std::make_unique<BaseLogFileWriter>(path, max_file_size_bytes);

  if (!result->Init()) {
    // Error logged by Init.
    result.reset();  // Destructor deletes errored files.
  }

  return result;
}

std::unique_ptr<CompressedSizeEstimator>
DefaultGzippedSizeEstimator::Factory::Create() const {
  return std::make_unique<DefaultGzippedSizeEstimator>();
}

size_t DefaultGzippedSizeEstimator::EstimateCompressedSize(
    const std::string& input) const {
  // This estimation is not tight. Since we expect to produce logs of
  // several MBs, overshooting the estimation by one KB should be
  // very safe and still relatively efficient.
  constexpr size_t kOverheadOverUncompressedSizeBytes = 1000;
  return input.length() + kOverheadOverUncompressedSizeBytes;
}

GzipLogCompressorFactory::GzipLogCompressorFactory(
    std::unique_ptr<CompressedSizeEstimator::Factory> estimator_factory)
    : estimator_factory_(std::move(estimator_factory)) {}

GzipLogCompressorFactory::~GzipLogCompressorFactory() = default;

size_t GzipLogCompressorFactory::MinSizeBytes() const {
  return kGzipOverheadBytes;
}

std::unique_ptr<LogCompressor> GzipLogCompressorFactory::Create(
    std::optional<size_t> max_size_bytes) const {
  if (max_size_bytes.has_value() && max_size_bytes.value() < MinSizeBytes()) {
    LOG(WARNING) << "Max size (" << max_size_bytes.value()
                 << ") below minimum size (" << MinSizeBytes() << ").";
    return nullptr;
  }
  return std::make_unique<GzipLogCompressor>(max_size_bytes,
                                             estimator_factory_->Create());
}

GzippedLogFileWriterFactory::GzippedLogFileWriterFactory(
    std::unique_ptr<GzipLogCompressorFactory> gzip_compressor_factory)
    : gzip_compressor_factory_(std::move(gzip_compressor_factory)) {}

GzippedLogFileWriterFactory::~GzippedLogFileWriterFactory() = default;

size_t GzippedLogFileWriterFactory::MinFileSizeBytes() const {
  // Only the compression's own overhead is incurred.
  return gzip_compressor_factory_->MinSizeBytes();
}

base::FilePath::StringPieceType GzippedLogFileWriterFactory::Extension() const {
  return kWebRtcEventLogGzippedExtension;
}

std::unique_ptr<LogFileWriter> GzippedLogFileWriterFactory::Create(
    const base::FilePath& path,
    std::optional<size_t> max_file_size_bytes) const {
  if (max_file_size_bytes.has_value() &&
      max_file_size_bytes.value() < MinFileSizeBytes()) {
    LOG(WARNING) << "Size below allowed minimum.";
    return nullptr;
  }

  auto gzip_compressor = gzip_compressor_factory_->Create(max_file_size_bytes);
  if (!gzip_compressor) {
    // The factory itself will have logged an error.
    return nullptr;
  }

  auto result = std::make_unique<GzippedLogFileWriter>(
      path, max_file_size_bytes, std::move(gzip_compressor));

  if (!result->Init()) {
    // Error logged by Init.
    result.reset();  // Destructor deletes errored files.
  }

  return result;
}

// Create a random identifier of 32 hexadecimal (uppercase) characters.
std::string CreateWebRtcEventLogId() {
  // UnguessableToken's interface makes no promisses over case. We therefore
  // convert, even if the current implementation does not require it.
  std::string log_id =
      base::ToUpperASCII(base::UnguessableToken::Create().ToString());
  DCHECK_EQ(log_id.size(), kWebRtcEventLogIdLength);
  DCHECK_EQ(log_id.find_first_not_of("0123456789ABCDEF"), std::string::npos);
  return log_id;
}

BrowserContextId GetBrowserContextId(
    const content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return reinterpret_cast<BrowserContextId>(browser_context);
}

BrowserContextId GetBrowserContextId(int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* const host =
      content::RenderProcessHost::FromID(render_process_id);

  content::BrowserContext* const browser_context =
      host ? host->GetBrowserContext() : nullptr;

  return GetBrowserContextId(browser_context);
}

base::FilePath GetRemoteBoundWebRtcEventLogsDir(
    const base::FilePath& browser_context_dir) {
  const base::FilePath::CharType kRemoteBoundLogSubDirectory[] =
      FILE_PATH_LITERAL("webrtc_event_logs");
  return browser_context_dir.Append(kRemoteBoundLogSubDirectory);
}

base::FilePath WebRtcEventLogPath(
    const base::FilePath& remote_logs_dir,
    const std::string& log_id,
    size_t web_app_id,
    const base::FilePath::StringPieceType& extension) {
  DCHECK_GE(web_app_id, kMinWebRtcEventLogWebAppId);
  DCHECK_LE(web_app_id, kMaxWebRtcEventLogWebAppId);

  static_assert(kWebAppIdLength == 2u, "Fix the code below.");
  const std::string web_app_id_str = base::StringPrintf("%02zu", web_app_id);
  DCHECK_EQ(web_app_id_str.length(), kWebAppIdLength);

  const std::string filename =
      std::string(kRemoteBoundWebRtcEventLogFileNamePrefix) + "_" +
      web_app_id_str + "_" + log_id;

  return remote_logs_dir.AppendASCII(filename).AddExtension(extension);
}

bool IsValidRemoteBoundLogFilename(const std::string& filename) {
  // The -1 is because of the implict \0.
  const size_t kPrefixLength =
      std::size(kRemoteBoundWebRtcEventLogFileNamePrefix) - 1;

  // [prefix]_[web_app_id]_[log_id]
  const size_t expected_length =
      kPrefixLength + 1 + kWebAppIdLength + 1 + kWebRtcEventLogIdLength;
  if (filename.length() != expected_length) {
    return false;
  }

  size_t index = 0;

  // Expect prefix.
  if (filename.find(kRemoteBoundWebRtcEventLogFileNamePrefix) != index) {
    return false;
  }
  index += kPrefixLength;

  // Expect underscore between prefix and web-app ID.
  if (filename[index] != '_') {
    return false;
  }
  index += 1;

  // Expect web-app-ID.
  const size_t web_app_id =
      ExtractWebAppId(std::string_view(&filename[index], kWebAppIdLength));
  if (web_app_id == kInvalidWebRtcEventLogWebAppId) {
    return false;
  }
  index += kWebAppIdLength;

  // Expect underscore between web-app ID and log ID.
  if (filename[index] != '_') {
    return false;
  }
  index += 1;

  // Expect log ID.
  const std::string log_id = filename.substr(index);
  DCHECK_EQ(log_id.length(), kWebRtcEventLogIdLength);
  return base::ContainsOnlyChars(log_id, "0123456789ABCDEF");
}

bool IsValidRemoteBoundLogFilePath(const base::FilePath& path) {
  const std::string filename = path.BaseName().RemoveExtension().MaybeAsASCII();
  return IsValidRemoteBoundLogFilename(filename);
}

base::FilePath GetWebRtcEventLogHistoryFilePath(const base::FilePath& path) {
  // TODO(crbug.com/40545136): Check for validity (after fixing unit tests).
  return path.RemoveExtension().AddExtension(kWebRtcEventLogHistoryExtension);
}

std::string ExtractRemoteBoundWebRtcEventLogLocalIdFromPath(
    const base::FilePath& path) {
  const std::string filename = path.BaseName().RemoveExtension().MaybeAsASCII();
  if (!IsValidRemoteBoundLogFilename(filename)) {
    LOG(WARNING) << "Invalid remote-bound WebRTC event log filename.";
    return std::string();
  }

  DCHECK_GE(filename.length(), kWebRtcEventLogIdLength);
  return filename.substr(filename.length() - kWebRtcEventLogIdLength);
}

size_t ExtractRemoteBoundWebRtcEventLogWebAppIdFromPath(
    const base::FilePath& path) {
  const std::string filename = path.BaseName().RemoveExtension().MaybeAsASCII();
  if (!IsValidRemoteBoundLogFilename(filename)) {
    LOG(WARNING) << "Invalid remote-bound WebRTC event log filename.";
    return kInvalidWebRtcEventLogWebAppId;
  }

  // The -1 is because of the implict \0.
  const size_t kPrefixLength =
      std::size(kRemoteBoundWebRtcEventLogFileNamePrefix) - 1;

  // The +1 is for the underscore between the prefix and the web-app ID.
  // Length verified by above call to IsValidRemoteBoundLogFilename().
  DCHECK_GE(filename.length(), kPrefixLength + 1 + kWebAppIdLength);
  std::string_view id_str(&filename[kPrefixLength + 1], kWebAppIdLength);

  return ExtractWebAppId(id_str);
}

bool DoesProfileDefaultToLoggingEnabled(const Profile* const profile) {
// For Chrome OS, exclude special profiles and users.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  // We do not log an error here since this can happen in several cases,
  // e.g. for signin profiles or lock screen app profiles.
  if (!user) {
    return false;
  }
  const user_manager::UserType user_type = user->GetType();
  if (user_type != user_manager::UserType::kRegular) {
    return false;
  }
  if (ash::ProfileHelper::IsEphemeralUserProfile(profile)) {
    return false;
  }
#endif

  // We only want a default of true for regular (i.e. logged-in) profiles
  // receiving cloud-based user-level enterprise policies. Supervised (child)
  // profiles are considered regular and can also receive cloud policies in some
  // cases (e.g. on Chrome OS). Although currently this should be covered by the
  // other checks, let's explicitly check to anticipate edge cases and make the
  // requirement explicit.
  if (profile->IsOffTheRecord() || profile->IsChild()) {
    return false;
  }

  const policy::ProfilePolicyConnector* const policy_connector =
      profile->GetProfilePolicyConnector();

  return policy_connector->policy_service()->IsInitializationComplete(
             policy::POLICY_DOMAIN_CHROME) &&
         policy_connector->IsManaged();
}

}  // namespace webrtc_event_logging
