// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/tab_url_provider.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// An implementation of optimization_guide::TabUrlProvider that provides tab
// URLs that are currently shown for the profile.
class OptimizationGuideTabUrlProvider
    : public optimization_guide::TabUrlProvider {
 public:
  explicit OptimizationGuideTabUrlProvider(Profile* profile);
  ~OptimizationGuideTabUrlProvider() override;

  // optimization_guide::TabUrlProvider:
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override;

 private:
  // Returns all web contents shown across all browser windows for |profile|.
  virtual const std::vector<content::WebContents*> GetAllWebContentsForProfile(
      Profile* profile);

  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_H_
