// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_WEB_CONTENTS_DELEGATE_UTIL_H_
#define CHROME_BROWSER_GLIC_WIDGET_WEB_CONTENTS_DELEGATE_UTIL_H_

#include "base/feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace glic {

// Sets (or clears, if `delegate` is nullptr) the delegate on `web_contents`.
// In NoWebview mode, this sets `delegate` as the embedder delegate on the
// owning PrivilegedWebContents. In WebUI mode, it sets `delegate` on
// `web_contents`. If `expected_delegate` is provided (non-null), the update is
// only applied if the current delegate matches `expected_delegate`.
template <typename DelegateType, typename ExpectedType = void*>
void SetWebContentsDelegate(content::WebContents* web_contents,
                            DelegateType delegate,
                            ExpectedType expected_delegate = nullptr) {
  if (!web_contents) {
    return;
  }
  if (base::FeatureList::IsEnabled(features::kGlicNoWebview)) {
    if (auto* pwc = pwc::PrivilegedWebContents::FromWebContents(web_contents)) {
      if (!expected_delegate || pwc->embedder_delegate() == expected_delegate) {
        pwc->SetEmbedderDelegate(delegate);
      }
      return;
    }
  }
  if (!expected_delegate || web_contents->GetDelegate() == expected_delegate) {
    web_contents->SetDelegate(delegate);
  }
}

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_WEB_CONTENTS_DELEGATE_UTIL_H_
