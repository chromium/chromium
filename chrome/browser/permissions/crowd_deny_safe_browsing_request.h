// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CROWD_DENY_SAFE_BROWSING_REQUEST_H_
#define CHROME_BROWSER_PERMISSIONS_CROWD_DENY_SAFE_BROWSING_REQUEST_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class Clock;
}

namespace url {
class Origin;
}

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

// Represents a single request to the Safe Browsing service to fetch the crowd
// deny verdict for a given origin. Can be created and used on any one thread.
class CrowdDenySafeBrowsingRequest {
 public:
  // The crowd deny verdict for a given origin.
  //
  // These enumeration values are recorded into histograms. Entries should not
  // be renumbered and numeric values should not be reused.
  enum class Verdict {
    kAcceptable = 0,
    kUnacceptable = 1,

    // Must be equal to the greatest among enumeraiton values.
    kMaxValue = kUnacceptable,
  };

  using VerdictCallback = base::OnceCallback<void(Verdict)>;

  // Constructs a request that fetches the verdict for |origin| by consulting
  // the |database_manager|, and invokes |callback| when done. The |clock| is
  // used for measuring how long the request takes, and should outlive |this|.
  //
  // It is guaranteed that |callback| will never be invoked synchronously, and
  // it will not be invoked after |this| goes out of scope.
  CrowdDenySafeBrowsingRequest(
      scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
          database_manager,
      const base::Clock* clock,
      const url::Origin& origin,
      VerdictCallback callback);
  ~CrowdDenySafeBrowsingRequest();

 private:
  class SafeBrowsingClient;

  CrowdDenySafeBrowsingRequest(const CrowdDenySafeBrowsingRequest&) = delete;
  CrowdDenySafeBrowsingRequest& operator=(const CrowdDenySafeBrowsingRequest&) =
      delete;

  // Posted by the |client_| when it gets a response.
  void OnReceivedResult(Verdict verdict);

  // The client interfacing with Safe Browsing.
  std::unique_ptr<SafeBrowsingClient> client_;

  VerdictCallback callback_;

  // For telemetry purposes. The caller guarantees |clock_| to outlive |this|.
  raw_ptr<const base::Clock> clock_;
  const base::Time request_start_time_;

  base::WeakPtrFactory<CrowdDenySafeBrowsingRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PERMISSIONS_CROWD_DENY_SAFE_BROWSING_REQUEST_H_
