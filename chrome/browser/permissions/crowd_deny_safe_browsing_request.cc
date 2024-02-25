// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/crowd_deny_safe_browsing_request.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace {

// The permission identifier string used by Safe Browsing for notifications.
constexpr char kSafeBrowsingNotificationPermissionName[] = "NOTIFICATIONS";

// The maximum amount of time to wait for the Safe Browsing response.
constexpr base::TimeDelta kSafeBrowsingCheckTimeout = base::Seconds(2);

}  // namespace

// CrowdDenySafeBrowsingRequest::SafeBrowsingClient --------------------------

class CrowdDenySafeBrowsingRequest::SafeBrowsingClient
    : public safe_browsing::SafeBrowsingDatabaseManager::Client {
 public:
  SafeBrowsingClient(scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
                         database_manager,
                     base::WeakPtr<CrowdDenySafeBrowsingRequest> handler,
                     scoped_refptr<base::TaskRunner> handler_task_runner)
      : database_manager_(database_manager),
        handler_(handler),
        handler_task_runner_(handler_task_runner) {}

  ~SafeBrowsingClient() override {
    if (timeout_.IsRunning())
      database_manager_->CancelApiCheck(this);
  }

  void CheckOrigin(const url::Origin& origin) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Start the timer before the call to CheckApiBlocklistUrl(), as it may
    // call back into OnCheckApiBlocklistUrlResult() synchronously.
    timeout_.Start(FROM_HERE, kSafeBrowsingCheckTimeout, this,
                   &SafeBrowsingClient::OnTimeout);

    if (database_manager_->CheckApiBlocklistUrl(origin.GetURL(), this)) {
      timeout_.AbandonAndStop();
      SendResultToHandler(Verdict::kAcceptable);
    }
  }

 private:
  SafeBrowsingClient(const SafeBrowsingClient&) = delete;
  SafeBrowsingClient& operator=(const SafeBrowsingClient&) = delete;

  static Verdict ExtractVerdictFromMetadata(
      const safe_browsing::ThreatMetadata& metadata) {
    return metadata.api_permissions.count(
               kSafeBrowsingNotificationPermissionName)
               ? Verdict::kUnacceptable
               : Verdict::kAcceptable;
  }

  void OnTimeout() {
    database_manager_->CancelApiCheck(this);
    SendResultToHandler(Verdict::kAcceptable);
  }

  void SendResultToHandler(Verdict verdict) {
    handler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CrowdDenySafeBrowsingRequest::OnReceivedResult,
                       handler_, verdict));
  }

  // SafeBrowsingDatabaseManager::Client:
  void OnCheckApiBlocklistUrlResult(
      const GURL& url,
      const safe_browsing::ThreatMetadata& metadata) override {
    timeout_.AbandonAndStop();
    SendResultToHandler(ExtractVerdictFromMetadata(metadata));
  }

  base::OneShotTimer timeout_;
  scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager_;
  base::WeakPtr<CrowdDenySafeBrowsingRequest> handler_;
  scoped_refptr<base::TaskRunner> handler_task_runner_;
};

// CrowdDenySafeBrowsingRequest ----------------------------------------------

CrowdDenySafeBrowsingRequest::CrowdDenySafeBrowsingRequest(
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> database_manager,
    const base::Clock* clock,
    const url::Origin& origin,
    VerdictCallback callback)
    : callback_(std::move(callback)),
      clock_(clock),
      request_start_time_(clock->Now()) {
  client_ = std::make_unique<SafeBrowsingClient>(
      database_manager, weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
  client_->CheckOrigin(origin);
}

CrowdDenySafeBrowsingRequest::~CrowdDenySafeBrowsingRequest() {
}

void CrowdDenySafeBrowsingRequest::OnReceivedResult(Verdict verdict) {
  base::UmaHistogramTimes("Permissions.CrowdDeny.SafeBrowsing.RequestDuration",
                          clock_->Now() - request_start_time_);
  base::UmaHistogramEnumeration("Permissions.CrowdDeny.SafeBrowsing.Verdict",
                                verdict);

  DCHECK(callback_);
  std::move(callback_).Run(verdict);
}
