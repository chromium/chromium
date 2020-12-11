// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_OBSERVER_H_
#define CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_OBSERVER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/engagement/site_engagement_service.h"

namespace content {
class WebContents;
}

class GURL;
class SiteEngagementService;

class SiteEngagementObserver {
 public:
  // Called when the engagement for |url| loaded in |web_contents| is changed
  // to |score|, due to an event of type |type|. This method may be run on user
  // input, so observers *must not* perform any expensive tasks here.
  // |web_contents| may be null if the engagement has increased when |url| is
  // not in a tab, e.g. from a notification interaction.
  virtual void OnEngagementEvent(content::WebContents* web_contents,
                                 const GURL& url,
                                 double score,
                                 SiteEngagementService::EngagementType type) {}

 protected:
  explicit SiteEngagementObserver(SiteEngagementService* service);

  SiteEngagementObserver();

  virtual ~SiteEngagementObserver();

  // Returns the site engagement service which this object is observing.
  SiteEngagementService* GetSiteEngagementService() const;

  // Begin observing |service| for engagement increases.
  // To stop observing, call Observe(nullptr).
  void Observe(SiteEngagementService* service);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteEngagementServiceTest, Observers);
  friend class SiteEngagementService;

  SiteEngagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementObserver);
};

#endif  // CHROME_BROWSER_ENGAGEMENT_SITE_ENGAGEMENT_OBSERVER_H_
