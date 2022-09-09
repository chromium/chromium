// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_POLICY_POLICY_AUDITOR_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_POLICY_POLICY_AUDITOR_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::WebContents;

class PolicyAuditorBridge
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PolicyAuditorBridge> {
 public:
  PolicyAuditorBridge(const PolicyAuditorBridge&) = delete;
  PolicyAuditorBridge& operator=(const PolicyAuditorBridge&) = delete;

  ~PolicyAuditorBridge() override;

  static void CreateForWebContents(WebContents* web_contents);

 private:
  friend WebContentsUserData<PolicyAuditorBridge>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit PolicyAuditorBridge(
      content::WebContents* web_contents,
      ScopedJavaLocalRef<jobject>&& android_policy_auditor);

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  ScopedJavaGlobalRef<jobject> android_policy_auditor_;
};

#endif  // CHROME_BROWSER_ANDROID_POLICY_POLICY_AUDITOR_BRIDGE_H_
