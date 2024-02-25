// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

// Downloads the user's contact list from the server. NOTE: An instance should
// only be used once. All necessary parameters are passed to the constructor,
// and the download begins when Run() is called.
class NearbyShareContactDownloader {
 public:
  using SuccessCallback = base::OnceCallback<void(
      std::vector<nearby::sharing::proto::ContactRecord> contacts,
      uint32_t num_unreachable_contacts_filtered_out)>;
  using FailureCallback = base::OnceClosure;

  // |device_id|: The ID used by the Nearby server to differentiate multiple
  //              devices from the same account.
  // |success_callback|: Invoked if the full contact list is successfully
  //                     downloaded.
  // |failure_callback|: Invoked if the contact list download fails.
  NearbyShareContactDownloader(const std::string& device_id,
                               SuccessCallback success_callback,
                               FailureCallback failure_callback);

  virtual ~NearbyShareContactDownloader();

  // Starts the  contact list download.
  void Run();

 protected:
  const std::string& device_id() const { return device_id_; }

  virtual void OnRun() = 0;

  // Invokes the success callback with the input parameters.
  void Succeed(std::vector<nearby::sharing::proto::ContactRecord> contacts,
               uint32_t num_unreachable_contacts_filtered_out);

  // Invokes the failure callback.
  void Fail();

 private:
  bool was_run_ = false;
  const std::string device_id_;
  SuccessCallback success_callback_;
  FailureCallback failure_callback_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_NEARBY_SHARE_CONTACT_DOWNLOADER_H_
