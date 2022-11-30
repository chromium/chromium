// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_HISTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_HISTORY_H_

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/time/time.h"

namespace webrtc_event_logging {

// Writes a small history file to disk, which allows us to remember what logs
// were captured and uploaded, after they are uploaded (whether successfully or
// not), or after they ware pruned (if they expire before an upload opportunity
// presents itself).
class WebRtcEventLogHistoryFileWriter final {
 public:
  // Creates and initializes a WebRtcEventLogHistoryFileWriter object.
  // Overwrites existing files on disk, if any.
  // If initialization fails (e.g. couldn't create the file), an empty
  // unique_ptr is returned.
  static std::unique_ptr<WebRtcEventLogHistoryFileWriter> Create(
      const base::FilePath& path);

  WebRtcEventLogHistoryFileWriter(const WebRtcEventLogHistoryFileWriter&) =
      delete;
  WebRtcEventLogHistoryFileWriter& operator=(
      const WebRtcEventLogHistoryFileWriter&) = delete;

  // The capture time must be later than UNIX epoch start.
  bool WriteCaptureTime(base::Time capture_time);

  // The upload time must be later than UNIX epoch start.
  // Writing an upload time earlier than the capture time is not prevented,
  // but an invalid history file will be produced.
  bool WriteUploadTime(base::Time upload_time);

  // If |upload_id| is empty, it means the upload was not successful. In that
  // case, the |upload_time| still denotes the time when the upload started.
  // |upload_id|'s length must not exceed kWebRtcEventLogMaxUploadIdBytes.
  bool WriteUploadId(const std::string& upload_id);

  // Deletes the file being written to, and invalidates this object.
  void Delete();

  // May only be called on a valid object.
  base::FilePath path() const;

 private:
  explicit WebRtcEventLogHistoryFileWriter(const base::FilePath& path);

  // Returns true if initialization was successful; false otherwise.
  // Overwrites existing files on disk, if any.
  bool Init();

  // Returns true if and only if the entire string was written to the file.
  bool Write(const std::string& str);

  const base::FilePath path_;
  base::File file_;
  bool valid_;
};

// Reads from disk a small history file and recovers the data from it.
class WebRtcEventLogHistoryFileReader final {
 public:
  // Creates and initializes a WebRtcEventLogHistoryFileReader object.
  // If initialization fails (e.g. couldn't parse the file), an empty
  // unique_ptr is returned.
  static std::unique_ptr<WebRtcEventLogHistoryFileReader> Create(
      const base::FilePath& path);

  WebRtcEventLogHistoryFileReader(const WebRtcEventLogHistoryFileReader&) =
      delete;
  WebRtcEventLogHistoryFileReader& operator=(
      const WebRtcEventLogHistoryFileReader&) = delete;

  WebRtcEventLogHistoryFileReader(WebRtcEventLogHistoryFileReader&& other);

  // Mandatory fields.
  std::string LocalId() const;     // Must return a non-empty ID.
  base::Time CaptureTime() const;  // Must return a non-null base::Time.

  // Optional fields.
  base::Time UploadTime() const;  // Non-null only if upload was attempted.
  std::string UploadId() const;   // Non-null only if upload was successful.

  // May only be performed on a valid object.
  base::FilePath path() const;

  // Compares by capture time.
  bool operator<(const WebRtcEventLogHistoryFileReader& other) const;

 private:
  explicit WebRtcEventLogHistoryFileReader(const base::FilePath& path);

  // Returns true if initialization was successful; false otherwise.
  // If true is returned, |this| is now valid, and will remain so until
  // the object is destroyed or std::move()-ed away from.
  bool Init();

  // Returns true if parsing succeeded; false otherwise.
  bool Parse(const std::string& file_contents);

  const base::FilePath path_;

  const std::string local_id_;

  // Mandatory field; must be non-null (and therefore also non-zero).
  base::Time capture_time_;

  // Optional fields; may appear 0 or 1 times in the file.
  base::Time upload_time_;  // Nullness/zero-ness indicates "unset".
  std::string upload_id_;   // Empty string indicates "unset".

  bool valid_;
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_HISTORY_H_
