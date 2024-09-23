// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_history.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace webrtc_event_logging {

// A sublcass of this interface will take ownership of a file, and either
// upload it to a remote server (actual implementation), or pretend to do so
// (in unit tests). Upon completion, success/failure will be reported by posting
// an UploadResultCallback task to the task queue on which this object lives.
class WebRtcEventLogUploader {
 public:
  using UploadResultCallback =
      base::OnceCallback<void(const base::FilePath& log_file,
                              bool upload_successful)>;

  // Since we'll need more than one instance of the abstract
  // WebRtcEventLogUploader, we'll need an abstract factory for it.
  class Factory {
   public:
    virtual ~Factory() = default;

    // Creates uploaders.
    // This takes ownership of the file. The caller must not attempt to access
    // the file after invoking Create(). Even deleting the file due to
    // the user clearing cache, is to be done through the uploader's Cancel,
    // and not through direct access of the caller to the file. The file may
    // be touched again only after |this| is destroyed.
    virtual std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        UploadResultCallback callback) = 0;
  };

  virtual ~WebRtcEventLogUploader() = default;

  // Getter for the details of the file this uploader is handling.
  // Can be called for ongoing, completed, failed or cancelled uploads.
  virtual const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const = 0;

  // Cancels the upload, then deletes the log file and its history file.
  // (These files are deleted even if the upload has been completed by the time
  // Cancel is called.)
  virtual void Cancel() = 0;
};

// Primary implementation of WebRtcEventLogUploader. Uploads log files to crash.
// Deletes log files whether they were successfully uploaded or not.
class WebRtcEventLogUploaderImpl : public WebRtcEventLogUploader {
 public:
  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    explicit Factory(scoped_refptr<base::SequencedTaskRunner> task_runner);

    ~Factory() override;

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        UploadResultCallback callback) override;

   protected:
    friend class WebRtcEventLogUploaderImplTest;

    std::unique_ptr<WebRtcEventLogUploader> CreateWithCustomMaxSizeForTesting(
        const WebRtcLogFileInfo& log_file,
        UploadResultCallback callback,
        size_t max_remote_log_file_size_bytes);

   private:
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
  };

  WebRtcEventLogUploaderImpl(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const WebRtcLogFileInfo& log_file,
      UploadResultCallback callback,
      size_t max_remote_log_file_size_bytes);
  ~WebRtcEventLogUploaderImpl() override;

  const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const override;

  void Cancel() override;

 private:
  friend class WebRtcEventLogUploaderImplTest;

  // Primes the log file for uploading. Returns true if the file could be read,
  // in which case |upload_data| will be populated with the data to be uploaded
  // (both the log file's contents as well as history for Crash).
  // TODO(crbug.com/40545136): Avoid reading the entire file into memory.
  bool PrepareUploadData(std::string* upload_data);

  // Initiates the file's upload.
  void StartUpload(const std::string& upload_data);

  // Callback invoked when the file upload has finished.
  // If the |url_loader_| instance it was bound to is deleted before
  // its invocation, the callback will not be called.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Cleanup and posting of the result callback.
  void ReportResult(bool upload_successful, bool delete_history_file = false);

  // Remove the log file which is owned by |this|.
  void DeleteLogFile();

  // Remove the log file which is owned by |this|.
  void DeleteHistoryFile();

  // The URL used for uploading the logs.
  static const char kUploadURL[];

  // The object lives on this IO-capable task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Housekeeping information about the uploaded file (path, time of last
  // modification, associated BrowserContext).
  const WebRtcLogFileInfo log_file_;

  // Callback posted back to signal success or failure.
  UploadResultCallback callback_;

  // Maximum allowed file size. In production code, this is a hard-coded,
  // but unit tests may set other values.
  const size_t max_log_file_size_bytes_;

  // Owns a history file which allows the state of the uploaded log to be
  // remembered after it has been uploaded and/or deleted.
  std::unique_ptr<WebRtcEventLogHistoryFileWriter> history_file_writer_;

  // This object is in charge of the actual upload.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Allows releasing |this| while a task from |url_loader_| is still pending.
  base::WeakPtrFactory<WebRtcEventLogUploaderImpl> weak_ptr_factory_{this};
};

}  // namespace webrtc_event_logging

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_
