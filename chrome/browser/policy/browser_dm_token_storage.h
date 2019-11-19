// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_H_
#define CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece_forward.h"
#include "base/system/sys_info.h"
#include "components/policy/core/common/cloud/dm_token.h"

namespace policy {

// Manages storing and retrieving tokens and client ID used to enroll browser
// instances for enterprise management. The tokens are read from disk or
// registry once and cached values are returned in subsequent calls.
//
// All calls to member functions must be sequenced. It is an error to attempt
// concurrent store operations. RetrieveClientId must be the first method
// called.
class BrowserDMTokenStorage {
 public:
  using StoreCallback = base::OnceCallback<void(bool success)>;

  // Returns the global singleton object. Must be called from the UI thread.
  // This implementation is platform dependant.
  static BrowserDMTokenStorage* Get();
  // Returns a client ID unique to the machine.
  std::string RetrieveClientId();
  // Returns the serial number of the machine.
  std::string RetrieveSerialNumber();
  // Returns the enrollment token, or an empty string if there is none.
  std::string RetrieveEnrollmentToken();
  // Asynchronously stores |dm_token| and calls |callback| with a boolean to
  // indicate success or failure. It is an error to attempt concurrent store
  // operations.
  void StoreDMToken(const std::string& dm_token, StoreCallback callback);
  // Returns an already stored DM token. An empty token is returned if no DM
  // token exists on the system or an error is encountered.
  // TODO(domfc): Remove overload after updating callers. Note that the names
  //              are different because you cannot overload functions that only
  //              differ in their return type.
  std::string RetrieveDMToken();
  DMToken RetrieveBrowserDMToken();
  // Must be called after the DM token is saved, to ensure that the callback is
  // invoked.
  void OnDMTokenStored(bool success);

  // Return true if we display error message dialog when enrollment process
  // fails.
  virtual bool ShouldDisplayErrorMessageOnFailure();

  // Set the mock BrowserDMTokenStorage for testing. The caller owns the
  // instance of the storage.
  static void SetForTesting(BrowserDMTokenStorage* storage) {
    storage_for_testing_ = storage;
  }

 protected:
  friend class base::NoDestructor<BrowserDMTokenStorage>;

  // Get the global singleton instance by calling BrowserDMTokenStorage::Get().
  BrowserDMTokenStorage();
  virtual ~BrowserDMTokenStorage();

 private:
  static BrowserDMTokenStorage* storage_for_testing_;

  // Initializes the DMTokenStorage object and caches the ids and tokens. This
  // is called the first time the BrowserDMTokenStorage is interacted with.
  void InitIfNeeded();

  // Gets the client ID and returns it. This implementation is platform
  // dependant.
  virtual std::string InitClientId() = 0;
  // Gets the client ID and returns it. This implementation is shared by all
  // platforms.
  std::string InitSerialNumber();
  // Gets the enrollment token and returns it. This implementation is platform
  // dependant.
  virtual std::string InitEnrollmentToken() = 0;
  // Gets the DM token and returns it. This implementation is platform
  // dependant.
  virtual std::string InitDMToken() = 0;
  // Gets the boolean value that determines if error message will be displayed
  // when enrollment fails.
  virtual bool InitEnrollmentErrorOption() = 0;
  // Saves the DM token. This implementation is platform dependant.
  virtual void SaveDMToken(const std::string& token) = 0;

  // Will be called after the DM token is stored.
  StoreCallback store_callback_;

  bool is_initialized_;

  std::string client_id_;
  base::Optional<std::string> serial_number_;
  std::string enrollment_token_;
  DMToken dm_token_;
  bool should_display_error_message_on_failure_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BrowserDMTokenStorage);
};

}  // namespace policy
#endif  // CHROME_BROWSER_POLICY_BROWSER_DM_TOKEN_STORAGE_H_
