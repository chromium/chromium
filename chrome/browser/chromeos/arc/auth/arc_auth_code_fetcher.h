// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CODE_FETCHER_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/chromeos/arc/auth/arc_fetcher_base.h"

namespace arc {

// Interface to implement auth code token fetching.
class ArcAuthCodeFetcher : public ArcFetcherBase {
 public:
  ~ArcAuthCodeFetcher() override = default;

  // Fetches the auth code in the background and calls |callback| when done.
  // |success| indicates whether the operation was successful. In case of
  // success, |auth_code| contains the auth code.
  // Fetch() should be called once per instance, and it is expected that
  // the inflight operation is cancelled without calling the |callback|
  // when the instance is deleted.
  // TODO(sinhak): Consider moving to |base::Optional<std::string>| for the
  // |auth_code| to avoid meaningless auth_code on error.
  using FetchCallback =
      base::OnceCallback<void(bool success, const std::string& auth_code)>;
  virtual void Fetch(FetchCallback callback) = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CODE_FETCHER_H_
