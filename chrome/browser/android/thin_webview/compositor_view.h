// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THIN_WEBVIEW_COMPOSITOR_VIEW_H_
#define CHROME_BROWSER_ANDROID_THIN_WEBVIEW_COMPOSITOR_VIEW_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace cc {
class Layer;
}  // namespace cc

namespace thin_webview {
namespace android {

// Native interface for the CompositorView.java.
class CompositorView {
 public:
  static CompositorView* FromJavaObject(
      const base::android::JavaRef<jobject>& jcompositor_view);

  // Called to set the root layer of the view.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> layer) = 0;
};

}  // namespace android
}  // namespace thin_webview

#endif  // CHROME_BROWSER_ANDROID_THIN_WEBVIEW_COMPOSITOR_VIEW_H_
