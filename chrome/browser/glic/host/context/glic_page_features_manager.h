// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_FEATURES_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_FEATURES_MANAGER_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace content {
class NavigationHandle;
class Page;
class WebContents;
}  // namespace content

namespace glic {

// Observes a WebContents to detect specific lightweight page features,
// caching the results for quick synchronous retrieval:
//  - presence of the "Ask" button on YouTube via isolated world JavaScript
// injection
class GlicPageFeaturesManager : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(GlicPageFeaturesManager);

  // Configuration for detection checks.
  static constexpr base::TimeDelta kCheckDelay = base::Seconds(3);
  static constexpr int kMaxRetries = 1;

  // LINT.IfChange(YoutubeSummarizeVideoZSS)
  enum class YoutubeSummarizeVideoZSS {
    kButtonFoundOnFirstCheck = 0,
    kButtonFoundOnSecondCheck = 1,
    kButtonNotFoundAfterAllChecks = 2,
    kMaxValue = kButtonNotFoundAfterAllChecks,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicYoutubeSummarizeVideoZSS)

  explicit GlicPageFeaturesManager(tabs::TabInterface* tab);
  ~GlicPageFeaturesManager() override;

  GlicPageFeaturesManager(const GlicPageFeaturesManager&) = delete;
  GlicPageFeaturesManager& operator=(const GlicPageFeaturesManager&) = delete;

  // Returns the currently cached features for this page.
  const std::vector<mojom::LightweightPageFeature>& GetFeatures() const;

  static GlicPageFeaturesManager* From(tabs::TabInterface* tab);

  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

 protected:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;

  void MaybeScheduleCheck();
  void ScheduleCheck();
  void RunCheck();
  void OnCheckResult(base::Value result);

  raw_ptr<tabs::TabInterface> const tab_;
  std::vector<mojom::LightweightPageFeature> cached_features_;
  base::OneShotTimer check_timer_;
  int check_count_ = 0;

  std::optional<ui::ScopedUnownedUserData<GlicPageFeaturesManager>>
      registration_;
  base::CallbackListSubscription tab_subscription_;

  base::WeakPtrFactory<GlicPageFeaturesManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_PAGE_FEATURES_MANAGER_H_
