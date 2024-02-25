// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_URL_SB_CLIENT_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_URL_SB_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

class DownloadProtectionService;
class SafeBrowsingUIManager;

// SafeBrowsing::Client class used to lookup the bad binary URL list.

class DownloadUrlSBClient : public SafeBrowsingDatabaseManager::Client,
                            public download::DownloadItem::Observer,
                            public base::RefCountedThreadSafe<
                                DownloadUrlSBClient,
                                content::BrowserThread::DeleteOnUIThread> {
 public:
  DownloadUrlSBClient(
      download::DownloadItem* item,
      DownloadProtectionService* service,
      CheckDownloadCallback callback,
      const scoped_refptr<SafeBrowsingUIManager>& ui_manager,
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager);

  DownloadUrlSBClient(const DownloadUrlSBClient&) = delete;
  DownloadUrlSBClient& operator=(const DownloadUrlSBClient&) = delete;

  // Implements DownloadItem::Observer.
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  void StartCheck();

  bool IsDangerous(SBThreatType threat_type) const;

  // Implements SafeBrowsingDatabaseManager::Client.
  void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                SBThreatType threat_type) override;

 private:
  friend class base::RefCountedThreadSafe<DownloadUrlSBClient>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<DownloadUrlSBClient>;

  ~DownloadUrlSBClient() override;

  void CheckDone(SBThreatType threat_type);

  void ReportMalware(SBThreatType threat_type);

  void IdentifyReferrerChain();

  void UpdateDownloadCheckStats(SBStatsType stat_type);

  void CreateAndMaybeSendClientSafeBrowsingWarningShownReport(
      std::string post_data);

  // The DownloadItem we are checking. Must be accessed only on UI thread.
  raw_ptr<download::DownloadItem> item_;

  // Copies of data from |item_| for access on other threads.
  std::string sha256_hash_;
  std::vector<GURL> url_chain_;
  GURL referrer_url_;

  // Enumeration types for histogram purposes.
  const SBStatsType total_type_;
  const SBStatsType dangerous_type_;

  raw_ptr<DownloadProtectionService, DanglingUntriaged> service_;
  CheckDownloadCallback callback_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  base::TimeTicks start_time_;
  ExtendedReportingLevel extended_reporting_level_;
  bool is_enhanced_protection_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      download_item_observation_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_URL_SB_CLIENT_H_
