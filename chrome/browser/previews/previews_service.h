// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "components/blacklist/opt_out_blacklist/opt_out_blacklist_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/top_host_provider.h"
#include "third_party/re2/src/re2/re2.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {
class OptimizationGuideService;
}

namespace previews {
class PreviewsUIService;
typedef std::vector<std::unique_ptr<re2::RE2>> RegexpList;
}  // namespace previews

namespace leveldb_proto {
class ProtoDatabaseProvider;
}

class PreviewsOfflineHelper;

// Keyed service that owns a previews::PreviewsUIService. PreviewsService lives
// on the UI thread.
class PreviewsService : public KeyedService {
 public:
  explicit PreviewsService(content::BrowserContext* browser_context);
  ~PreviewsService() override;

  // Initializes the UI Service. |optimization_guide_service| is the
  // Optimization Guide Service that is being listened to and is guaranteed to
  // outlive |this|. |ui_task_runner| is the UI thread task runner.
  // |profile_path| is the path to user data on disc.
  void Initialize(
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const base::FilePath& profile_path);

  // Allows the |previews_lite_page_redirect_decider_| to remove itself from
  // observed classes.
  void Shutdown() override;

  // Clears the history of the black lists in |previews_ui_service_| and
  // |previews_lite_page_redirect_decider_| between |begin_time| and |end_time|.
  void ClearBlackList(base::Time begin_time, base::Time end_time);

  // The previews UI thread service.
  previews::PreviewsUIService* previews_ui_service() {
    return previews_ui_service_.get();
  }

  // The server lite page preview decider.
  PreviewsLitePageRedirectDecider* previews_lite_page_redirect_decider() {
    return previews_lite_page_redirect_decider_.get();
  }

  // The https notification infobar decider.
  PreviewsHTTPSNotificationInfoBarDecider*
  previews_https_notification_infobar_decider() {
    return previews_lite_page_redirect_decider_.get();
  }

  PreviewsOfflineHelper* previews_offline_helper() {
    return previews_offline_helper_.get();
  }

  // Returns the enabled PreviewsTypes with their version.
  static blacklist::BlacklistData::AllowedTypesAndVersions GetAllowedPreviews();

  // Called when that there is a redirect from |start_url| to |end_url|. Called
  // only when DeferAllScript preview feature is enabled.
  void ReportObservedRedirectWithDeferAllScriptPreview(const GURL& start_url,
                                                       const GURL& end_url);

  // Returns true if |url| is marked as eligible for defer all script preview.
  bool IsUrlEligibleForDeferAllScriptPreview(const GURL& url) const;

  // Returns true if |start_url| leads to a URL redirect cycle based on
  // |redirect_history|.
  static bool HasURLRedirectCycle(
      const GURL& start_url,
      const base::MRUCache<GURL, GURL>& redirect_history);

  // Returns true if |url| patially matches any of the regular expressions for
  // which DeferAllScript preview can't be shown.
  bool MatchesDeferAllScriptDenyListRegexp(const GURL& url) const;

  optimization_guide::TopHostProvider* GetTopHostProviderForTesting() const {
    return top_host_provider_.get();
  }

 private:
  // The top site provider for use with the Previews Optimization Guide's Hints
  // Fetcher.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  // The previews UI thread service.
  std::unique_ptr<previews::PreviewsUIService> previews_ui_service_;

  // The server lite page preview decider.
  std::unique_ptr<PreviewsLitePageRedirectDecider>
      previews_lite_page_redirect_decider_;

  // The offline previews helper.
  std::unique_ptr<PreviewsOfflineHelper> previews_offline_helper_;

  // Guaranteed to outlive |this|.
  content::BrowserContext* browser_context_;

  // URL Factory for the Previews Optimization Guide's Hints Fetcher.
  scoped_refptr<network::SharedURLLoaderFactory>
      optimization_guide_url_loader_factory_;

  // Stores history of URL redirects. Key is the starting URL and value is the
  // URL that the starting URL redirected to. Populated only when DeferAllScript
  // preview feature is enabled.
  base::MRUCache<GURL, GURL> redirect_history_;

  // A given URL is ineligible for defer preview if it partially matches any of
  // the regular expression in |defer_all_script_denylist_regexps_|.
  const std::unique_ptr<previews::RegexpList>
      defer_all_script_denylist_regexps_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsService);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_
