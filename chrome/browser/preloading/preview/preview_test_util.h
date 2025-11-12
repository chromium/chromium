// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TEST_UTIL_H_
#define CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TEST_UTIL_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class PreviewManager;

namespace test {

// Enables appropriate features for the LinkPreview.
class ScopedPreviewFeatureList {
 public:
  ScopedPreviewFeatureList();
  ScopedPreviewFeatureList(const ScopedPreviewFeatureList&) = delete;
  ScopedPreviewFeatureList& operator=(const ScopedPreviewFeatureList&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Helper class to control the LinkPreview feature in browser tests.
class PreviewTestHelper {
 public:
  class Waiter {
   public:
    virtual void Wait() {}
  };
  explicit PreviewTestHelper(const content::WebContents::Getter& fn);
  ~PreviewTestHelper();

  PreviewManager& GetManager();
  base::WeakPtr<content::WebContents> GetWebContentsForPreviewTab();
  void InitiatePreview(const GURL& url);
  void PromoteToNewTab();
  void WaitUntilLoadFinished();

  Waiter CreateActivationWaiter();

  // Tentative helper method until the primary page navigation closes existing
  // preview pages.
  void CloseAndWaitUntilFinished();

 private:
  content::WebContents::Getter get_web_contents_fn_;
  ScopedPreviewFeatureList scoped_feature_list_;
};

}  // namespace test

#endif  // CHROME_BROWSER_PRELOADING_PREVIEW_PREVIEW_TEST_UTIL_H_
