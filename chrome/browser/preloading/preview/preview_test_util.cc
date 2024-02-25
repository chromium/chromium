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

class EventWaiter final : public PreviewTestHelper::Waiter,
                          public content::WebContentsObserver {
 public:
  enum class Event {
    kLoad,
    kActivation,
    kClose,
  };
  EventWaiter(content::WebContents* web_contents, Event event)
      : WebContentsObserver(web_contents), event_(event) {
    CHECK(web_contents);
    if (event == Event::kLoad) {
      done_ = !web_contents->IsLoading();
    }
  }

  // PreviewtestHelper::Waiter:
  void Wait() override {
    if (!done_) {
      run_loop_.Run();
    }
  }

 private:
  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    if (event_ == Event::kLoad) {
      run_loop_.Quit();
    }
  }
  void DidActivatePreviewedPage(base::TimeTicks activation_time) override {
    if (event_ == Event::kActivation) {
      run_loop_.Quit();
    }
  }
  void WebContentsDestroyed() override {
    if (event_ == Event::kClose) {
      run_loop_.Quit();
    }
  }

  base::RunLoop run_loop_;
  Event event_;
  bool done_ = false;
};

}  // namespace

ScopedPreviewFeatureList::ScopedPreviewFeatureList() {
  feature_list_.InitAndEnableFeature(blink::features::kLinkPreview);
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
  EventWaiter event_waiter(web_contents.get(), EventWaiter::Event::kLoad);
  event_waiter.Wait();
}

PreviewTestHelper::Waiter PreviewTestHelper::CreateActivationWaiter() {
  base::WeakPtr<content::WebContents> web_contents =
      GetManager().GetWebContentsForPreviewTab();
  return EventWaiter(web_contents.get(), EventWaiter::Event::kActivation);
}

void PreviewTestHelper::CloseAndWaitUntilFinished() {
  base::WeakPtr<content::WebContents> web_contents =
      GetManager().GetWebContentsForPreviewTab();
  EventWaiter event_waiter(web_contents.get(), EventWaiter::Event::kClose);
  GetManager().CloseForTesting();
  event_waiter.Wait();
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
