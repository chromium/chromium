// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TRIGGER_CREATOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_TRIGGER_CREATOR_H_

#include <memory>

class Profile;

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace safe_browsing {

class AdSamplerTrigger;
class SuspiciousSiteTrigger;
class TriggerManagerWebContentsHelper;

// Takes care of creation and ownership of individual Safe Browsing triggers for
// a tab. This functionality lives in a separate class from TriggerManager to
// avoid circular dependencies: TriggerManager need not know about individual
// trigger classes, while the trigger classes need to know about the
// TriggerManager in order to fire triggers.
class TriggerCreator {
 public:
  TriggerCreator(tabs::TabInterface& tab,
                 Profile* profile,
                 content::WebContents* web_contents);
  TriggerCreator(const TriggerCreator&) = delete;
  TriggerCreator& operator=(const TriggerCreator&) = delete;
  ~TriggerCreator();

 private:
  std::unique_ptr<TriggerManagerWebContentsHelper>
      trigger_manager_web_contents_helper_;
  std::unique_ptr<AdSamplerTrigger> ad_sampler_trigger_;
  std::unique_ptr<SuspiciousSiteTrigger> suspicious_site_trigger_;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_TRIGGER_CREATOR_H_
