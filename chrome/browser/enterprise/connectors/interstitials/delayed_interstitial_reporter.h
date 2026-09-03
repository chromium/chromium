// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_DELAYED_INTERSTITIAL_REPORTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_DELAYED_INTERSTITIAL_REPORTER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace enterprise_data_protection {

// Observer that waits for a WebContents to finish loading before
// reporting an enterprise interstitial event (like SafeBrowsing or
// URLFiltering) so that the correct page title can be extracted.
class DelayedInterstitialReporter
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DelayedInterstitialReporter> {
 public:
  using TitleCallback = base::OnceCallback<void(const std::string& tab_title)>;


  ~DelayedInterstitialReporter() override;

  DelayedInterstitialReporter(const DelayedInterstitialReporter&) = delete;
  DelayedInterstitialReporter& operator=(const DelayedInterstitialReporter&) =
      delete;

 private:
  friend class content::WebContentsUserData<DelayedInterstitialReporter>;
  friend class DelayedInterstitialReporterTest;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  static void Start(content::WebContents* web_contents,
                    TitleCallback report_callback,
                    bool is_bypassing_interstitial,
                    std::string uma_suffix);

  DelayedInterstitialReporter(content::WebContents* web_contents,
                              TitleCallback report_callback,
                              bool is_bypassing_interstitial,
                              std::string uma_suffix);

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void PrimaryPageChanged(content::Page& page) override;


  enum class RunState { kFailed, kTimeout, kSuccess };

  void OnTimeout();
  void RunCallbackAndCleanUp(RunState run_state);

  TitleCallback report_callback_;
  base::OneShotTimer timer_;
  std::string uma_suffix_;
  base::TimeTicks start_time_;
  bool is_bypassing_interstitial_ = false;
};

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_DELAYED_INTERSTITIAL_REPORTER_H_
