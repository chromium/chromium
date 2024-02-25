// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_DOWNLOADER_H_
#define CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_DOWNLOADER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_downloader_impl.h"

class NearbyShareClientFactory;

// A fake implementation of NearbyShareContactDownloader, along with a fake
// factory, to be used in tests. Call the exposed base class method Fail() or
// Succeed() with the desired results to invoke the result callback.
class FakeNearbyShareContactDownloader : public NearbyShareContactDownloader {
 public:
  // Factory that creates FakeNearbyShareContactDownloader instances. Use in
  // NearbyShareContactDownloaderImpl::Factory::SetFactoryForTesting() in unit
  // tests.
  class Factory : public NearbyShareContactDownloaderImpl::Factory {
   public:
    Factory();
    ~Factory() override;

    // Returns all FakeNearbyShareContactDownloader instances created by
    // CreateInstance().
    std::vector<raw_ptr<FakeNearbyShareContactDownloader, VectorExperimental>>&
    instances() {
      return instances_;
    }

    base::TimeDelta latest_timeout() const { return latest_timeout_; }

    NearbyShareClientFactory* latest_client_factory() const {
      return latest_client_factory_;
    }

   private:
    // NearbyShareContactDownloaderImpl::Factory:
    std::unique_ptr<NearbyShareContactDownloader> CreateInstance(
        const std::string& device_id,
        base::TimeDelta timeout,
        NearbyShareClientFactory* client_factory,
        SuccessCallback success_callback,
        FailureCallback failure_callback) override;

    std::vector<raw_ptr<FakeNearbyShareContactDownloader, VectorExperimental>>
        instances_;
    base::TimeDelta latest_timeout_;
    raw_ptr<NearbyShareClientFactory> latest_client_factory_;
  };

  FakeNearbyShareContactDownloader(const std::string& device_id,
                                   SuccessCallback success_callback,
                                   FailureCallback failure_callback);
  ~FakeNearbyShareContactDownloader() override;

  // NearbyShareContactDownloader:
  void OnRun() override;

  // Make protected methods from base class public in this fake class.
  using NearbyShareContactDownloader::device_id;
  using NearbyShareContactDownloader::Fail;
  using NearbyShareContactDownloader::Succeed;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CONTACTS_FAKE_NEARBY_SHARE_CONTACT_DOWNLOADER_H_
