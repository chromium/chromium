// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/helpers/page_live_state_decorator_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace performance_manager {
namespace {

#if !BUILDFLAG(IS_ANDROID)
// Encapsulates all of the "Active tab" tracking logic, which uses `BrowserList`
// and is therefore not available on Android. This class keeps track of existing
// Browsers and their tab strips, and updates PageLiveState data with whether
// each tab is currently active or not.
class ActiveTabObserver : public TabStripModelObserver,
                          public BrowserListObserver {
 public:
  ActiveTabObserver() {
    BrowserList::AddObserver(this);
    for (Browser* browser : *BrowserList::GetInstance()) {
      AddBrowserTabStripObservation(browser);
    }
  }

  ~ActiveTabObserver() override { BrowserList::RemoveObserver(this); }

 private:
  void AddBrowserTabStripObservation(Browser* browser) {
    browser->tab_strip_model()->AddObserver(this);
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (selection.active_tab_changed() && !tab_strip_model->empty()) {
      if (selection.old_contents) {
        PageLiveStateDecorator::SetIsActiveTab(selection.old_contents, false);
      }
      if (selection.new_contents) {
        PageLiveStateDecorator::SetIsActiveTab(selection.new_contents, true);
      }
    }

    if (change.type() == TabStripModelChange::kInserted) {
      for (const TabStripModelChange::ContentsWithIndex& tab :
           change.GetInsert()->contents) {
        // Pinned tabs can be restored from previous session in pinned state
        // and hence won't trigger a pinned state changed event
        PageLiveStateDecorator::SetIsPinnedTab(
            tab.contents, tab_strip_model->IsTabPinned(tab.index));
      }
    }
  }

  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PageLiveStateDecorator::SetIsPinnedTab(contents,
                                           tab_strip_model->IsTabPinned(index));
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    AddBrowserTabStripObservation(browser);
  }

  void OnBrowserRemoved(Browser* browser) override {
    browser->tab_strip_model()->RemoveObserver(this);
  }

  SEQUENCE_CHECKER(sequence_checker_);
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
// Listens to content::WebContentsObserver notifications for a given WebContents
// and updates the PageLiveStateDecorator accordingly. Destroys itself when the
// WebContents it observes is destroyed.
class PageLiveStateDecoratorHelper::WebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* web_contents,
                               PageLiveStateDecoratorHelper* outer)
      : content::WebContentsObserver(web_contents),
        outer_(outer),
        prev_(nullptr),
        next_(outer->first_web_contents_observer_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (next_) {
      DCHECK(!next_->prev_);
      next_->prev_ = this;
    }
    outer_->first_web_contents_observer_ = this;
  }

  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;

  ~WebContentsObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // content::WebContentsObserver:
  void OnDeviceConnectionTypesChanged(DeviceConnectionType connection_type,
                                      bool used) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PageLiveStateDecorator::OnDeviceConnectionTypesChanged(
        web_contents(), connection_type, used);
  }

  void WebContentsDestroyed() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DetachAndDestroy();
  }

  // Removes the WebContentsObserver from the linked list and deletes it.
  void DetachAndDestroy() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (prev_) {
      DCHECK_EQ(prev_->next_, this);
      prev_->next_ = next_;
    } else {
      DCHECK_EQ(outer_->first_web_contents_observer_, this);
      outer_->first_web_contents_observer_ = next_;
    }
    if (next_) {
      DCHECK_EQ(next_->prev_, this);
      next_->prev_ = prev_;
    }

    delete this;
  }

 private:
  const raw_ptr<PageLiveStateDecoratorHelper> outer_;
  raw_ptr<WebContentsObserver> prev_;
  raw_ptr<WebContentsObserver> next_;

  SEQUENCE_CHECKER(sequence_checker_);
};

PageLiveStateDecoratorHelper::PageLiveStateDecoratorHelper() {
  PerformanceManager::AddObserver(this);

  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->AddObserver(this);

#if !BUILDFLAG(IS_ANDROID)
  active_tab_observer_ = std::make_unique<ActiveTabObserver>();
#endif  // !BUILDFLAG(IS_ANDROID)

  content::DevToolsAgentHost::AddObserver(this);
}

PageLiveStateDecoratorHelper::~PageLiveStateDecoratorHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::DevToolsAgentHost::RemoveObserver(this);

  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->RemoveObserver(this);

  // Destroy all WebContentsObserver to ensure that PageLiveStateDecorators are
  // no longer maintained.
  while (first_web_contents_observer_)
    first_web_contents_observer_->DetachAndDestroy();

  PerformanceManager::RemoveObserver(this);
}

void PageLiveStateDecoratorHelper::OnIsCapturingVideoChanged(
    content::WebContents* contents,
    bool is_capturing_video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsCapturingVideoChanged(contents,
                                                    is_capturing_video);
}

void PageLiveStateDecoratorHelper::OnIsCapturingAudioChanged(
    content::WebContents* contents,
    bool is_capturing_audio) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsCapturingAudioChanged(contents,
                                                    is_capturing_audio);
}

void PageLiveStateDecoratorHelper::OnIsBeingMirroredChanged(
    content::WebContents* contents,
    bool is_being_mirrored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsBeingMirroredChanged(contents, is_being_mirrored);
}

void PageLiveStateDecoratorHelper::OnIsCapturingWindowChanged(
    content::WebContents* contents,
    bool is_capturing_window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsCapturingWindowChanged(contents,
                                                     is_capturing_window);
}

void PageLiveStateDecoratorHelper::OnIsCapturingDisplayChanged(
    content::WebContents* contents,
    bool is_capturing_display) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageLiveStateDecorator::OnIsCapturingDisplayChanged(contents,
                                                      is_capturing_display);
}

void PageLiveStateDecoratorHelper::DevToolsAgentHostAttached(
    content::DevToolsAgentHost* agent_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (agent_host->GetType() == content::DevToolsAgentHost::kTypePage &&
      agent_host->GetWebContents() != nullptr) {
    PageLiveStateDecorator::SetIsDevToolsOpen(agent_host->GetWebContents(),
                                              true);
  }
}

void PageLiveStateDecoratorHelper::DevToolsAgentHostDetached(
    content::DevToolsAgentHost* agent_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (agent_host->GetType() == content::DevToolsAgentHost::kTypePage &&
      agent_host->GetWebContents() != nullptr) {
    PageLiveStateDecorator::SetIsDevToolsOpen(agent_host->GetWebContents(),
                                              false);
  }
}

void PageLiveStateDecoratorHelper::OnPageNodeCreatedForWebContents(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_contents);
  // Start observing the WebContents. See comment on
  // |first_web_contents_observer_| for lifetime management details.
  new WebContentsObserver(web_contents, this);
  PageLiveStateDecorator::SetWasDiscarded(web_contents,
                                          web_contents->WasDiscarded());
}

}  // namespace performance_manager
