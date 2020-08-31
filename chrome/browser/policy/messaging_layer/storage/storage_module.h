// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_MODULE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_MODULE_H_

#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/policy/messaging_layer/encryption/encryption_module.h"
#include "chrome/browser/policy/messaging_layer/storage/storage.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

class StorageModule : public base::RefCountedThreadSafe<StorageModule> {
 public:
  // Factory method creates |StorageModule| object.
  static void Create(
      const Storage::Options& options,
      Storage::StartUploadCb start_upload_cb,
      scoped_refptr<EncryptionModule> encryption_module,
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)>
          callback);

  StorageModule(const StorageModule& other) = delete;
  StorageModule& operator=(const StorageModule& other) = delete;

  // AddRecord will add |record| (taking ownership) to the |StorageModule|
  // according to the provided |priority|. On completion, |callback| will be
  // called.
  virtual void AddRecord(Priority priority,
                         Record record,
                         base::OnceCallback<void(Status)> callback);

  // Once a record has been successfully uploaded, the sequencing information
  // can be passed back to the StorageModule here for record deletion.
  virtual void ReportSuccess(SequencingInformation sequencing_information);

 protected:
  // Constructor can only be called by |Create| factory method.
  StorageModule();

  // Refcounted object must have destructor declared protected or private.
  virtual ~StorageModule();

 private:
  friend base::RefCountedThreadSafe<StorageModule>;

  // Storage backend (currently only Storage).
  // TODO(b/160334561): make it a pluggable interface.
  scoped_refptr<Storage> storage_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_STORAGE_MODULE_H_
