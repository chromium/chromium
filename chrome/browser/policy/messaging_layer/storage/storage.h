// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_queue.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

// Storage represents the data to be collected, stored persistently and uploaded
// according to the priority.
class Storage : public base::RefCountedThreadSafe<Storage> {
 public:
  // Interface for Upload, which must be implemented by an object returned by
  // |StartUpload| callback (see below).
  // Every time Storage starts an upload (by timer or immediately after Write)
  // it uses this interface to hand available records over to the actual
  // uploader. Storage takes ownership of it and automatically discards after
  // |Completed| returns. Similar to StorageQueue::UploaderInterface, but with
  // added priority parameter.
  class UploaderInterface {
   public:
    virtual ~UploaderInterface() = default;

    // Unserializes every record and hands ownership over for processing (e.g.
    // to add to the network message). Expects |processed_cb| to be called after
    // the record or error status has been processed, with true if next record
    // needs to be delivered and false if the Uploader should stop.
    virtual void ProcessRecord(StatusOr<EncryptedRecord> record,
                               base::OnceCallback<void(bool)> processed_cb) = 0;

    // Finalizes the upload (e.g. sends the message to the server and gets
    // response).
    virtual void Completed(Status final_status) = 0;
  };

  // Callback type for UploadInterface provider for specified queue.
  using StartUploadCb =
      base::RepeatingCallback<StatusOr<std::unique_ptr<UploaderInterface>>(
          Priority priority)>;

  // Options class allowing to set parameters individually, e.g.:
  // Storage::Create(Options()
  //                     .set_directory("/var/cache/reporting"),
  //                 callback);
  class Options {
   public:
    Options() = default;
    Options(const Options& options) = default;
    Options& operator=(const Options& options) = default;
    Options& set_directory(const base::FilePath& directory) {
      directory_ = directory;
      return *this;
    }
    const base::FilePath& directory() const { return directory_; }

   private:
    // Subdirectory of the location assigned for this Storage.
    base::FilePath directory_;
  };

  // Creates Storage instance, and returns it with the completion callback.
  static void Create(
      const Options& options,
      StartUploadCb start_upload_cb,
      scoped_refptr<EncryptionModule> encryption_module,
      base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> completion_cb);

  // Wraps and serializes Record (taking ownership of it), encrypts and writes
  // the resulting blob into the Storage (the last file of it) according to the
  // priority with the next sequencing number assigned. If file is going to
  // become too large, it is closed and new file is created.
  void Write(Priority priority,
             Record record,
             base::OnceCallback<void(Status)> completion_cb);

  // Confirms acceptance of the records according to the priority up to
  // |seq_number| (inclusively). All records with sequeincing numbers <= this
  // one can be removed from the Storage, and can no longer be uploaded.
  void Confirm(Priority priority,
               uint64_t seq_number,
               base::OnceCallback<void(Status)> completion_cb);

  // Initiates upload of collected records according to the priority.
  // Called usually for a queue with an infinite or very large upload period.
  // Multiple |Flush| calls can safely run in parallel.
  // Returns error if cannot start upload.
  Status Flush(Priority priority);

  Storage(const Storage& other) = delete;
  Storage& operator=(const Storage& other) = delete;

 protected:
  virtual ~Storage();

 private:
  friend class base::RefCountedThreadSafe<Storage>;

  // Private bridge class.
  class QueueUploaderInterface;

  // Private constructor, to be called by Create factory method only.
  // Queues need to be added afterwards.
  Storage(const Options& options, StartUploadCb start_upload_cb);

  // Initializes the object by adding all queues for all priorities.
  // Must be called once and only once after construction.
  // Returns OK or error status, if anything failed to initialize.
  Status Init();

  // Helper method that selects queue by priority. Returns error
  // if priority does not match any queue.
  // Note: queues_ never change after initialization is finished, so there is no
  // need to protect or serialize access to it.
  StatusOr<scoped_refptr<StorageQueue>> GetQueue(Priority priority);

  const Options options_;

  // Map priority->StorageQueue.
  base::flat_map<Priority, scoped_refptr<StorageQueue>> queues_;

  // Upload provider callback.
  const StartUploadCb start_upload_cb_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_H_
