// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/webrtc_event_log_history.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

namespace webrtc_event_logging {

const size_t kWebRtcEventLogMaxUploadIdBytes = 100;

namespace {
// Compactness is not important for these few and small files; we therefore
// go with a human-readable format.
const char kCaptureTimeLinePrefix[] =
    "Capture time (seconds since UNIX epoch): ";
const char kUploadTimeLinePrefix[] = "Upload time (seconds since UNIX epoch): ";
const char kUploadIdLinePrefix[] = "Upload ID: ";

// No need to use \r\n for Windows; better have a consistent file format
// between platforms.
const char kEOL[] = "\n";
static_assert(std::size(kEOL) == 1 + 1 /* +1 for the implicit \0. */,
              "SplitString relies on this being a single character.");

// |time| must *not* be earlier than UNIX epoch start. If it is, the empty
// string is returned.
std::string DeltaFromEpochSeconds(base::Time time) {
  if (time.is_null() || time.is_min() || time.is_max()) {
    LOG(ERROR) << "Not a valid time (" << time << ").";
    return std::string();
  }

  const base::Time epoch = base::Time::UnixEpoch();
  if (time < epoch) {
    LOG(WARNING) << "Time to go back to the future.";
    return std::string();
  }

  return base::NumberToString((time - epoch).InSeconds());
}

// Helper for ParseTime; see its documentation for details.
base::Time StringToTime(const std::string& time) {
  int64_t seconds_from_epoch;
  if (!base::StringToInt64(time, &seconds_from_epoch) ||
      seconds_from_epoch < 0) {
    LOG(WARNING) << "Error encountered while reading time.";
    return base::Time();
  }

  return base::Time::UnixEpoch() + base::Seconds(seconds_from_epoch);
}

// Convert a history file's timestamp, which is the number of seconds since
// UNIX epoch, into a base::Time object.
// This function errors on timestamps from UNIX epoch or before it.
bool ParseTime(const std::string& line,
               const std::string& prefix,
               base::Time* out) {
  DCHECK(line.find(prefix) == 0);
  DCHECK(out);

  if (!out->is_null()) {
    LOG(WARNING) << "Repeated line.";
    return false;
  }

  const base::Time time = StringToTime(line.substr(prefix.length()));
  if (time.is_null()) {
    LOG(WARNING) << "Null time.";
    return false;
  }

  *out = time;

  return true;
}

bool ParseString(const std::string& line,
                 const std::string& prefix,
                 std::string* out) {
  DCHECK(line.find(prefix) == 0);
  DCHECK(out);

  if (!out->empty()) {
    LOG(WARNING) << "Repeated line.";
    return false;
  }

  *out = line.substr(prefix.length());

  if (out->empty()) {
    LOG(WARNING) << "Empty string.";
    return false;
  }

  return true;
}
}  // namespace

std::unique_ptr<WebRtcEventLogHistoryFileWriter>
WebRtcEventLogHistoryFileWriter::Create(const base::FilePath& path) {
  auto history_file_writer =
      base::WrapUnique(new WebRtcEventLogHistoryFileWriter(path));
  if (!history_file_writer->Init()) {
    LOG(WARNING) << "Initialization of history file writer failed.";
    return nullptr;
  }
  return history_file_writer;
}

WebRtcEventLogHistoryFileWriter::WebRtcEventLogHistoryFileWriter(
    const base::FilePath& path)
    : path_(path), valid_(false) {}

bool WebRtcEventLogHistoryFileWriter::Init() {
  DCHECK(!valid_);

  if (base::PathExists(path_)) {
    if (!base::DeleteFile(path_)) {
      LOG(ERROR) << "History file already exists, and could not be deleted.";
      return false;
    }
    LOG(WARNING) << "History file already existed; deleted.";
  }

  // Attempt to create the file.
  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  file_.Initialize(path_, file_flags);
  if (!file_.IsValid() || !file_.created()) {
    LOG(WARNING) << "Couldn't create history file.";
    if (!base::DeleteFile(path_)) {
      LOG(ERROR) << "Failed to delete " << path_ << ".";
    }
    return false;
  }

  valid_ = true;
  return true;
}

bool WebRtcEventLogHistoryFileWriter::WriteCaptureTime(
    base::Time capture_time) {
  DCHECK(valid_);

  if (capture_time.is_null()) {
    valid_ = false;
    return false;
  }

  const std::string delta_seconds = DeltaFromEpochSeconds(capture_time);
  if (delta_seconds.empty()) {
    valid_ = false;
    return false;
  }

  const bool written = Write(kCaptureTimeLinePrefix + delta_seconds + kEOL);
  if (!written) {
    // Error logged by Write().
    valid_ = false;
    return false;
  }

  return true;
}

bool WebRtcEventLogHistoryFileWriter::WriteUploadTime(base::Time upload_time) {
  DCHECK(valid_);

  if (upload_time.is_null()) {
    valid_ = false;
    return false;
  }

  const std::string delta_seconds = DeltaFromEpochSeconds(upload_time);
  if (delta_seconds.empty()) {
    valid_ = false;
    return false;
  }

  const bool written = Write(kUploadTimeLinePrefix + delta_seconds + kEOL);
  if (!written) {
    valid_ = false;
    return false;
  }

  return true;
}

bool WebRtcEventLogHistoryFileWriter::WriteUploadId(
    const std::string& upload_id) {
  DCHECK(valid_);
  DCHECK(!upload_id.empty());
  DCHECK_LE(upload_id.length(), kWebRtcEventLogMaxUploadIdBytes);

  const bool written = Write(kUploadIdLinePrefix + upload_id + kEOL);
  if (!written) {
    valid_ = false;
    return false;
  }

  return true;
}

void WebRtcEventLogHistoryFileWriter::Delete() {
  if (!base::DeleteFile(path_)) {
    LOG(ERROR) << "History file could not be deleted.";
  }

  valid_ = false;  // Like was already false.
}

base::FilePath WebRtcEventLogHistoryFileWriter::path() const {
  DCHECK(valid_);  // Can be performed on invalid objects, but likely shouldn't.
  return path_;
}

bool WebRtcEventLogHistoryFileWriter::Write(const std::string& str) {
  DCHECK(valid_);
  DCHECK(!str.empty());
  DCHECK_LE(str.length(), static_cast<size_t>(std::numeric_limits<int>::max()));

  if (!file_.WriteAtCurrentPosAndCheck(base::as_byte_span(str))) {
    LOG(WARNING) << "Writing to history file failed.";
    valid_ = false;
    return false;
  }

  // Writes to the history file are infrequent, and happen on a |task_runner_|
  // dedicated to event logs. We can therefore afford to Flush() after every
  // write, giving us greater confidence that information would not get lost if,
  // e.g., Chrome crashes.
  file_.Flush();

  return true;
}

std::unique_ptr<WebRtcEventLogHistoryFileReader>
WebRtcEventLogHistoryFileReader::Create(const base::FilePath& path) {
  auto history_file_reader =
      base::WrapUnique(new WebRtcEventLogHistoryFileReader(path));
  if (!history_file_reader->Init()) {
    LOG(WARNING) << "Initialization of history file reader failed.";
    return nullptr;
  }
  return history_file_reader;
}

WebRtcEventLogHistoryFileReader::WebRtcEventLogHistoryFileReader(
    const base::FilePath& path)
    : path_(path),
      local_id_(ExtractRemoteBoundWebRtcEventLogLocalIdFromPath(path_)),
      valid_(false) {}

WebRtcEventLogHistoryFileReader::WebRtcEventLogHistoryFileReader(
    WebRtcEventLogHistoryFileReader&& other)
    : path_(other.path_),
      local_id_(other.local_id_),
      capture_time_(other.capture_time_),
      upload_time_(other.upload_time_),
      upload_id_(other.upload_id_),
      valid_(other.valid_) {
  other.valid_ = false;
}

bool WebRtcEventLogHistoryFileReader::Init() {
  DCHECK(!valid_);

  if (local_id_.empty()) {
    LOG(WARNING) << "Unknown local ID.";
    return false;
  }

  if (local_id_.length() > kWebRtcEventLogMaxUploadIdBytes) {
    LOG(WARNING) << "Excessively long local ID.";
    return false;
  }

  if (!base::PathExists(path_)) {
    LOG(WARNING) << "File does not exist.";
    return false;
  }

  constexpr int file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
  base::File file(path_, file_flags);
  if (!file.IsValid()) {
    LOG(WARNING) << "Couldn't read history file.";
    if (!base::DeleteFile(path_)) {
      LOG(ERROR) << "Failed to delete " << path_ << ".";
    }
    return false;
  }

  constexpr size_t kMaxHistoryFileSizeBytes = 1024;
  static_assert(kWebRtcEventLogMaxUploadIdBytes < kMaxHistoryFileSizeBytes, "");

  std::string file_contents;
  file_contents.resize(kMaxHistoryFileSizeBytes);
  const int read_bytes = file.Read(0, &file_contents[0], file_contents.size());
  if (read_bytes < 0) {
    LOG(WARNING) << "Couldn't read contents of history file.";
    return false;
  }
  DCHECK_LE(static_cast<size_t>(read_bytes), file_contents.size());
  file_contents.resize(static_cast<size_t>(read_bytes));
  // Note: In excessively long files, the rest of the file will be ignored; the
  // beginning of the file will encounter a parse error.

  if (!Parse(file_contents)) {
    LOG(WARNING) << "Parsing of history file failed.";
    return false;
  }

  valid_ = true;
  return true;
}

std::string WebRtcEventLogHistoryFileReader::LocalId() const {
  DCHECK(valid_);
  DCHECK(!local_id_.empty());
  return local_id_;
}

base::Time WebRtcEventLogHistoryFileReader::CaptureTime() const {
  DCHECK(valid_);
  DCHECK(!capture_time_.is_null());
  return capture_time_;
}

base::Time WebRtcEventLogHistoryFileReader::UploadTime() const {
  DCHECK(valid_);
  return upload_time_;  // May be null (which indicates "unset").
}

std::string WebRtcEventLogHistoryFileReader::UploadId() const {
  DCHECK(valid_);
  return upload_id_;
}

base::FilePath WebRtcEventLogHistoryFileReader::path() const {
  DCHECK(valid_);
  return path_;
}

bool WebRtcEventLogHistoryFileReader::operator<(
    const WebRtcEventLogHistoryFileReader& other) const {
  DCHECK(valid_);
  DCHECK(!capture_time_.is_null());
  DCHECK(other.valid_);
  DCHECK(!other.capture_time_.is_null());
  if (capture_time_ == other.capture_time_) {
    // Resolve ties arbitrarily, but consistently (Local IDs are unique).
    return LocalId() < other.LocalId();
  }
  return (capture_time_ < other.capture_time_);
}

bool WebRtcEventLogHistoryFileReader::Parse(const std::string& file_contents) {
  DCHECK(!valid_);
  DCHECK(capture_time_.is_null());
  DCHECK(upload_time_.is_null());
  DCHECK(upload_id_.empty());

  const std::vector<std::string> lines =
      base::SplitString(file_contents, kEOL, base::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);

  for (const std::string& line : lines) {
    if (line.find(kCaptureTimeLinePrefix) == 0) {
      if (!ParseTime(line, kCaptureTimeLinePrefix, &capture_time_)) {
        return false;
      }
    } else if (line.find(kUploadTimeLinePrefix) == 0) {
      if (!ParseTime(line, kUploadTimeLinePrefix, &upload_time_)) {
        return false;
      }
    } else if (line.find(kUploadIdLinePrefix) == 0) {
      if (!ParseString(line, kUploadIdLinePrefix, &upload_id_)) {
        return false;
      }
    } else {
      LOG(WARNING) << "Unrecognized line in history file.";
      return false;
    }
  }

  if (capture_time_.is_null()) {
    LOG(WARNING) << "Incomplete history file; capture time unknown.";
    return false;
  }

  if (!upload_id_.empty() && upload_time_.is_null()) {
    LOG(WARNING) << "Incomplete history file; upload time known, "
                 << "but ID unknown.";
    return false;
  }

  if (!upload_time_.is_null() && upload_time_ < capture_time_) {
    LOG(WARNING) << "Defective history file; claims to have been uploaded "
                 << "before being captured.";
    return false;
  }

  return true;
}

}  // namespace webrtc_event_logging
