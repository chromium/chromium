// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_test_util.h"

#include "base/run_loop.h"
#include "chrome/browser/preloading/preview/preview_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/features.h"

namespace test {

namespace {

class PageLoadWaiter final : public content::WebContentsObserver {
 public:
  explicit PageLoadWaiter(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    CHECK(web_contents);
  }

  void Wait() { run_loop_.Run(); }

 private:
  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
};

}  // namespace

ScopedPreviewFeatureList::ScopedPreviewFeatureList() {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{blink::features::kLinkPreview,
                            blink::features::kLinkPreviewNavigation},
      /*disabled_features=*/{});
}

PreviewTestHelper::PreviewTestHelper(const content::WebContents::Getter& fn)
    : get_web_contents_fn_(fn) {}

PreviewTestHelper::~PreviewTestHelper() = default;

void PreviewTestHelper::InitiatePreview(const GURL& url) {
  GetManager().InitiatePreview(url);
}

void PreviewTestHelper::WaitUntilLoadFinished() {
  base::WeakPtr<content::WebContents> web_contents =
      GetManager().GetWebContentsForPreviewTab();
  CHECK(web_contents);
  if (!web_contents->IsLoading()) {
    return;
  }
  PageLoadWaiter page_load_waiter(web_contents.get());
  page_load_waiter.Wait();
}

PreviewManager& PreviewTestHelper::GetManager() {
  content::WebContents* web_contents = get_web_contents_fn_.Run();
  CHECK(web_contents);
  PreviewManager::CreateForWebContents(web_contents);
  PreviewManager* manager = PreviewManager::FromWebContents(web_contents);
  CHECK(manager);
  return *manager;
}

}  // namespace test
