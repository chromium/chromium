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
    load_finished_ = !web_contents->IsLoading();
  }

  void Wait() { run_loop_.Run(); }

  bool load_finished() { return load_finished_; }
  bool activation_finished() { return activation_finished_; }
  bool close_finished() { return close_finished_; }

 private:
  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    load_finished_ = true;
    run_loop_.Quit();
  }
  void DidActivatePreviewedPage(base::TimeTicks activation_time) override {
    activation_finished_ = true;
    run_loop_.Quit();
  }
  void WebContentsDestroyed() override {
    close_finished_ = true;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool load_finished_ = false;
  bool activation_finished_ = false;
  bool close_finished_ = false;
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

base::WeakPtr<content::WebContents>
PreviewTestHelper::GetWebContentsForPreviewTab() {
  return GetManager().GetWebContentsForPreviewTab();
}

void PreviewTestHelper::InitiatePreview(const GURL& url) {
  GetManager().InitiatePreview(url);
}

void PreviewTestHelper::PromoteToNewTab() {
  GetManager().PromoteToNewTab();
}

void PreviewTestHelper::WaitUntilLoadFinished() {
  base::WeakPtr<content::WebContents> web_contents =
      GetManager().GetWebContentsForPreviewTab();
  PageLoadWaiter page_load_waiter(web_contents.get());
  while (!page_load_waiter.load_finished()) {
    page_load_waiter.Wait();
  }
}

void PreviewTestHelper::ActivateAndWaitUntilFinished() {
  base::WeakPtr<content::WebContents> web_contents =
      GetManager().GetWebContentsForPreviewTab();
  PageLoadWaiter page_load_waiter(web_contents.get());
  GetManager().ActivateForTesting();
  while (!page_load_waiter.activation_finished()) {
    page_load_waiter.Wait();
  }
}

void PreviewTestHelper::CloseAndWaitUntilFinished() {
  base::WeakPtr<content::WebContents> web_contents =
      GetManager().GetWebContentsForPreviewTab();
  PageLoadWaiter page_load_waiter(web_contents.get());
  GetManager().CloseForTesting();
  while (!page_load_waiter.close_finished()) {
    page_load_waiter.Wait();
  }
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
