// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ANDROID_TAB_SHARING_UI_ANDROID_H_
#define CHROME_BROWSER_MEDIA_ANDROID_TAB_SHARING_UI_ANDROID_H_

#include <memory>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"

// Android-specific implementation of MediaStreamUI for tab sharing state
// (capturee).
class TabSharingUIAndroid : public MediaStreamUI {
 public:
  explicit TabSharingUIAndroid(content::WebContents* capturer_web_contents,
                               const content::DesktopMediaID& media_id,
                               bool app_preferred_current_tab);
  ~TabSharingUIAndroid() override;

  // chrome::MediaStreamUI override.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;

  // Called via JNI from TabSharingUiBridge when the user clicks the "Stop
  // sharing" button on the toolbar, or internally upon teardown. Executes the
  // underlying WebRTC callback to stop capturing.
  void StopSharing();

  // Called via JNI from TabSharingUiBridge when the user chooses to switch the
  // shared tab within an active session (e.g., clicking "Switch to present this
  // tab" on the toolbar). Switches the active capture source to |new_source|.
  void ChangeSource(content::WebContents* new_source);

 private:
  base::WeakPtr<content::WebContents> capturer_web_contents_;
  base::OnceClosure stop_callback_;
  content::MediaStreamUI::SourceCallback source_callback_;
  const content::DesktopMediaID media_id_;
  const bool app_preferred_current_tab_;
  std::unique_ptr<content::MediaStreamUI> tab_capture_indicator_ui_;
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

#endif  // CHROME_BROWSER_MEDIA_ANDROID_TAB_SHARING_UI_ANDROID_H_
