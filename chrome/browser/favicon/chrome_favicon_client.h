// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_H_
#define CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_H_

#include "base/macros.h"
#include "components/favicon/core/favicon_client.h"

class GURL;
class Profile;

// ChromeFaviconClient implements the the FaviconClient interface.
class ChromeFaviconClient : public favicon::FaviconClient {
 public:
  explicit ChromeFaviconClient(Profile* profile);
  ~ChromeFaviconClient() override;

 private:
  // favicon::FaviconClient implementation:
  bool IsNativeApplicationURL(const GURL& url) override;
  bool IsReaderModeURL(const GURL& url) override;
  const GURL GetOriginalUrlFromReaderModeUrl(const GURL& url) override;
  base::CancelableTaskTracker::TaskId GetFaviconForNativeApplicationURL(
      const GURL& url,
      const std::vector<int>& desired_sizes_in_pixel,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFaviconClient);
};

#endif  // CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_H_
