// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/storage/storage_configuration.h"
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
  // Interface for Upload, forwarding to StorageQueue::UploaderInterface.
  using UploaderInterface = StorageQueue::UploaderInterface;

  // Callback type for UploadInterface provider for specified queue.
  using StartUploadCb =
      base::RepeatingCallback<StatusOr<std::unique_ptr<UploaderInterface>>(
          Priority priority)>;

  // Creates Storage instance, and returns it with the completion callback.
  static void Create(
      const StorageOptions& options,
      StartUploadCb start_upload_cb,
      scoped_refptr<EncryptionModule> encryption_module,
      base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> completion_cb);

  // Wraps and serializes Record (taking ownership of it), encrypts and writes
  // the resulting blob into the Storage (the last file of it) according to the
  // priority with the next sequencing id assigned. If file is going to
  // become too large, it is closed and new file is created.
  void Write(Priority priority,
             Record record,
             base::OnceCallback<void(Status)> completion_cb);

  // Confirms acceptance of the records according to the priority up to
  // |sequencing_id| (inclusively). All records with sequeincing ids <= this
  // one can be removed from the Storage, and can no longer be uploaded.
  void Confirm(Priority priority,
               uint64_t sequencing_id,
               base::OnceCallback<void(Status)> completion_cb);

  // Initiates upload of collected records according to the priority.
  // Called usually for a queue with an infinite or very large upload period.
  // Multiple |Flush| calls can safely run in parallel.
  // Returns error if cannot start upload.
  Status Flush(Priority priority);

  // If the server attached signed encryption key to the response, it needs to
  // be paased here.
  void UpdateEncryptionKey(SignedEncryptionInfo signed_encryption_key);

  // Returns `false` if encryption key has not been found in the Storage during
  // initialization and not received from the server yet, and `true` otherwise.
  // The result is lazy: the method may return `false` for some time even after
  // the key has already been set - this is harmless, since resetting or even
  // changing the key is OK at any time.
  bool has_encryption_key() const;

  Storage(const Storage& other) = delete;
  Storage& operator=(const Storage& other) = delete;

 protected:
  virtual ~Storage();

 private:
  friend class base::RefCountedThreadSafe<Storage>;

  // Private bridge class.
  class QueueUploaderInterface;

  // Private helper class for key upload/download to the file system.
  class KeyInStorage;

  // Private constructor, to be called by Create factory method only.
  // Queues need to be added afterwards.
  Storage(const StorageOptions& options,
          scoped_refptr<EncryptionModule> encryption_module,
          StartUploadCb start_upload_cb);

  // Initializes the object by adding all queues for all priorities.
  // Must be called once and only once after construction.
  // Returns OK or error status, if anything failed to initialize.
  Status Init();

  // Helper method that selects queue by priority. Returns error
  // if priority does not match any queue.
  // Note: queues_ never change after initialization is finished, so there is no
  // need to protect or serialize access to it.
  StatusOr<scoped_refptr<StorageQueue>> GetQueue(Priority priority);

  // Immutable options, stored at the time of creation.
  const StorageOptions options_;

  // Encryption module.
  scoped_refptr<EncryptionModule> encryption_module_;

  // Internal key management module.
  std::unique_ptr<KeyInStorage> key_in_storage_;

  // Map priority->StorageQueue.
  base::flat_map<Priority, scoped_refptr<StorageQueue>> queues_;

  // Upload provider callback.
  const StartUploadCb start_upload_cb_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_H_
