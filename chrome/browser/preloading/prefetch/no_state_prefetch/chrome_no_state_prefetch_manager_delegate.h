// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_MANAGER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager_delegate.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"

class Profile;

namespace content_settings {
class CookieSettings;
}

namespace prerender {

class ChromeNoStatePrefetchManagerDelegate
    : public NoStatePrefetchManagerDelegate {
 public:
  explicit ChromeNoStatePrefetchManagerDelegate(Profile* profile);
  ~ChromeNoStatePrefetchManagerDelegate() override = default;

  // NoStatePrefetchManagerDelegate overrides.
  scoped_refptr<content_settings::CookieSettings> GetCookieSettings() override;
  void MaybePreconnect(const GURL& url) override;
  std::unique_ptr<NoStatePrefetchContentsDelegate>
  GetNoStatePrefetchContentsDelegate() override;
  bool IsNetworkPredictionPreferenceEnabled() override;
  std::string GetReasonForDisablingPrediction() override;

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_NO_STATE_PREFETCH_CHROME_NO_STATE_PREFETCH_MANAGER_DELEGATE_H_
