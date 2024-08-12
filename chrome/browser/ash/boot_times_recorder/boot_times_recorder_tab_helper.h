// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOOT_TIMES_RECORDER_BOOT_TIMES_RECORDER_TAB_HELPER_H_
#define CHROME_BROWSER_ASH_BOOT_TIMES_RECORDER_BOOT_TIMES_RECORDER_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ash {

// During login, notifies `BootTimesRecorder` whenever a tab starts or ends
// loading.
class BootTimesRecorderTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BootTimesRecorderTabHelper> {
 public:
  // Creates BootTimesRecorderTabHelper and attaches it to the `web_contents` if
  // login is not done yet.
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  BootTimesRecorderTabHelper(const BootTimesRecorderTabHelper&) = delete;
  BootTimesRecorderTabHelper& operator=(const BootTimesRecorderTabHelper&) =
      delete;

  ~BootTimesRecorderTabHelper() override;

  // content::WebContentsObserver:
  void DidStartLoading() override;
  void DidStopLoading() override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override;

 private:
  friend class content::WebContentsUserData<BootTimesRecorderTabHelper>;

  explicit BootTimesRecorderTabHelper(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOOT_TIMES_RECORDER_BOOT_TIMES_RECORDER_TAB_HELPER_H_
