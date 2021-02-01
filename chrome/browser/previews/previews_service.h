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
#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/re2/src/re2/re2.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace previews {
class PreviewsUIService;
typedef std::vector<std::unique_ptr<re2::RE2>> RegexpList;
}  // namespace previews

// Keyed service that owns a previews::PreviewsUIService. PreviewsService lives
// on the UI thread.
class PreviewsService : public KeyedService {
 public:
  explicit PreviewsService(content::BrowserContext* browser_context);
  ~PreviewsService() override;

  // Initializes the UI Service. |ui_task_runner| is the UI thread task runner.
  // |profile_path| is the path to user data on disc.
  void Initialize(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const base::FilePath& profile_path);

  // Allows members to remove themselves from observed classes.
  void Shutdown() override;

  // Clears the history of the block lists in |previews_ui_service_| and between
  // |begin_time| and |end_time|.
  void ClearBlockList(base::Time begin_time, base::Time end_time);

  // The previews UI thread service.
  previews::PreviewsUIService* previews_ui_service() {
    return previews_ui_service_.get();
  }

  // The https notification infobar decider.
  PreviewsHTTPSNotificationInfoBarDecider*
  previews_https_notification_infobar_decider() {
    return previews_https_notification_infobar_decider_.get();
  }

  // Returns the enabled PreviewsTypes with their version.
  static blocklist::BlocklistData::AllowedTypesAndVersions GetAllowedPreviews();

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

 private:
  // The previews UI thread service.
  std::unique_ptr<previews::PreviewsUIService> previews_ui_service_;

  // The decider for showing the HTTPS Notification InfoBar.
  std::unique_ptr<PreviewsHTTPSNotificationInfoBarDecider>
      previews_https_notification_infobar_decider_;

  // Guaranteed to outlive |this|.
  content::BrowserContext* browser_context_;

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
