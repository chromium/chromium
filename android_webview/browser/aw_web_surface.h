// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_SURFACE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace android_webview {

// Native counterpart of Java AwWebSurface. Bridges overscroll between
// BrowserViewRenderer and the Java AwWebSurface's EventListener.
//
// Owned by WebContents as user data while an AwWebSurface is attached.
//
// Lifetime: WebView
class AwWebSurface : public content::WebContentsUserData<AwWebSurface> {
 public:
  AwWebSurface(const AwWebSurface&) = delete;
  AwWebSurface& operator=(const AwWebSurface&) = delete;

  ~AwWebSurface() override;

  // Forwards unconsumed physical scroll delta and fling velocity to the Java
  // EventListener.
  void DidOverscroll(const gfx::Vector2dF& physical_overscroll_delta,
                     const gfx::Vector2dF& physical_fling_velocity);

 private:
  friend class content::WebContentsUserData<AwWebSurface>;

  AwWebSurface(content::WebContents* web_contents,
               const base::android::JavaRef<jobject>& obj);

  base::android::ScopedJavaGlobalRef<jobject> java_surface_;

  base::WeakPtrFactory<AwWebSurface> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_SURFACE_H_
