// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOCUMENT_DOCUMENT_WEB_CONTENTS_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOCUMENT_DOCUMENT_WEB_CONTENTS_DELEGATE_H_

#include <stdint.h>

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"

// Stub WebContentsDelegateAndroid that is meant to be a temporary substitute
// for a real WebContentsDelegate for the (expectedly short) period between when
// a new WebContents is created and the new DocumentActivity/DocumentTab are
// created and take ownership of the WebContents (which replaces this Delegate
// with a real one).  It is not meant to do anything except allow
// WebContentsDelegateAndroid::OpenURLFromTab() to load the URL for the
// WebContents.
class DocumentWebContentsDelegate
    : public web_contents_delegate_android::WebContentsDelegateAndroid {
 public:
  DocumentWebContentsDelegate(JNIEnv* env, jobject obj);
  ~DocumentWebContentsDelegate() override;

  // Attaches this delegate to the given WebContents.
  void AttachContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& jweb_contents);

  // Overridden from WebContentsDelegate.
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture,
                      bool* was_blocked) override;
  void CloseContents(content::WebContents* source) override;
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
};

#endif  // CHROME_BROWSER_ANDROID_DOCUMENT_DOCUMENT_WEB_CONTENTS_DELEGATE_H_
