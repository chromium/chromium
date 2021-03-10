// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/page_freezer.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_result.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"

namespace performance_manager {
namespace mechanism {
namespace {

const char kFreezingAttemptOutcomeHistogramName[] =
    "Tabs.Freezing.MaybeFreezeOutcome";

// Possible outcome of the |MaybeFreezePageOnUIThreadImpl| function.
//
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "FreezingAttemptOutcome" in src/tools/metrics/histograms/enums.xml.
enum class UMAFreezingAttemptOutcome {
  kSuccess = 0,
  kFailureContentsDoesntExist = 1,
  kFailureDueToNotificationPermission = 2,
  kFailureDueToNoStoreCacheHeaders = 3,

  kMaxValue = kFailureDueToNoStoreCacheHeaders,
};

// Try to freeze a page on the UI thread.
UMAFreezingAttemptOutcome MaybeFreezePageOnUIThreadImpl(
    const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const contents = contents_proxy.Get();
  if (!contents)
    return UMAFreezingAttemptOutcome::kFailureContentsDoesntExist;

  // Page with the notification permission shouldn't be frozen as this is a
  // strong signal that the user wants to receive updates from this page while
  // it's in background. This information isn't available in the PM graph, this
  // has to be checked on the UI thread.
  auto notif_permission =
      PermissionManagerFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()))
          ->GetPermissionStatus(ContentSettingsType::NOTIFICATIONS,
                                contents->GetLastCommittedURL(),
                                contents->GetLastCommittedURL());
  if (notif_permission.content_setting == CONTENT_SETTING_ALLOW)
    return UMAFreezingAttemptOutcome::kFailureDueToNotificationPermission;

  // Pages that have the "Cache-Control: no-store" header should not be frozen
  // because it could introduce a small privacy regression when returning to
  // the tab after it has been frozen in background for a long time (e.g. the
  // content of a banking tab could be visible for a few frames before the
  // site realizing that the user should have been logged out a while ago).
  auto* response_headers = contents->GetMainFrame()->GetLastResponseHeaders();
  if (response_headers && response_headers->HasHeaderValue(
                              net::HttpRequestHeaders::kCacheControl,
                              net::HttpRequestHeaders::kNoStoreDirective)) {
    return UMAFreezingAttemptOutcome::kFailureDueToNoStoreCacheHeaders;
  }

  contents->SetPageFrozen(true);
  return UMAFreezingAttemptOutcome::kSuccess;
}

bool MaybeFreezePageOnUIThread(const WebContentsProxy& contents_proxy) {
  auto freeze_outcome = MaybeFreezePageOnUIThreadImpl(contents_proxy);

  base::UmaHistogramEnumeration(kFreezingAttemptOutcomeHistogramName,
                                freeze_outcome);

  return freeze_outcome == UMAFreezingAttemptOutcome::kSuccess;
}

void UnfreezePageOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const content = contents_proxy.Get();
  if (!content)
    return;

  // A visible page is automatically unfrozen.
  if (content->GetVisibility() == content::Visibility::VISIBLE)
    return;

  content->SetPageFrozen(false);
}

}  // namespace

void PageFreezer::MaybeFreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(&MaybeFreezePageOnUIThread),
                                page_node->GetContentsProxy()));
}

void PageFreezer::UnfreezePageNode(const PageNode* page_node) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&UnfreezePageOnUIThread, page_node->GetContentsProxy()));
}

void PageFreezer::MaybeFreezePageNodeWithReplyForTesting(
    const PageNode* page_node,
    base::OnceCallback<void(bool)> reply_cb) {
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MaybeFreezePageOnUIThread, page_node->GetContentsProxy()),
      std::move(reply_cb));
}

}  // namespace mechanism
}  // namespace performance_manager
