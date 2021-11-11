// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_MEDIA_LICENSE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_MEDIA_LICENSE_HELPER_H_

#include <stdint.h>
#include <list>

#include "base/callback.h"
#include "chrome/browser/browsing_data/browsing_data_media_license_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

class MockBrowsingDataMediaLicenseHelper
    : public BrowsingDataMediaLicenseHelper {
 public:
  explicit MockBrowsingDataMediaLicenseHelper(Profile* profile);

  MockBrowsingDataMediaLicenseHelper(
      const MockBrowsingDataMediaLicenseHelper&) = delete;
  MockBrowsingDataMediaLicenseHelper& operator=(
      const MockBrowsingDataMediaLicenseHelper&) = delete;

  // BrowsingDataMediaLicenseHelper implementation:
  void StartFetching(FetchCallback callback) override;
  void DeleteMediaLicenseOrigin(const GURL& origin) override;

  // Add some MediaLicenseInfo samples.
  void AddMediaLicenseSamples();

  // Notifies the callback.
  void Notify();

  // Returns true if the origin list is empty.
  bool AllDeleted();

 protected:
  ~MockBrowsingDataMediaLicenseHelper() override;

 private:
  FetchCallback callback_;
  std::list<MediaLicenseInfo> media_licenses_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_MOCK_BROWSING_DATA_MEDIA_LICENSE_HELPER_H_
