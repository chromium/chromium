// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THIN_WEBVIEW_CHROME_THIN_WEBVIEW_INITIALIZER_H_
#define CHROME_BROWSER_ANDROID_THIN_WEBVIEW_CHROME_THIN_WEBVIEW_INITIALIZER_H_

#include "components/thin_webview/thin_webview_initializer.h"

namespace thin_webview::android {

// A helper class to help in attaching tab helpers.
class ChromeThinWebViewInitializer : public ThinWebViewInitializer {
 public:
  static void Initialize();

  ChromeThinWebViewInitializer() = default;

  ChromeThinWebViewInitializer(const ChromeThinWebViewInitializer&) = delete;
  ChromeThinWebViewInitializer& operator=(const ChromeThinWebViewInitializer&) =
      delete;

  ~ChromeThinWebViewInitializer() = default;

  void SetUpTheming(content::WebContents* web_contents) override;

  void AttachTabHelpers(content::WebContents* web_contents,
                        bool enable_permission_requests) override;

  void SetContextMenuPopulatorFactory(
      content::WebContents* web_contents,
      const base::android::JavaRef<jobject>& jpopulator_factory) override;
};

}  // namespace thin_webview::android

#endif  // CHROME_BROWSER_ANDROID_THIN_WEBVIEW_CHROME_THIN_WEBVIEW_INITIALIZER_H_
