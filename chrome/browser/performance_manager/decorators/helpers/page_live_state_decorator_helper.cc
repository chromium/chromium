// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/helpers/page_live_state_decorator_helper.h"

#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents_observer.h"

namespace performance_manager {

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
  void OnIsConnectedToBluetoothDeviceChanged(
      bool is_connected_to_bluetooth_device) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    PageLiveStateDecorator::OnIsConnectedToBluetoothDeviceChanged(
        web_contents(), is_connected_to_bluetooth_device);
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
  PageLiveStateDecoratorHelper* const outer_;
  WebContentsObserver* prev_;
  WebContentsObserver* next_;

  SEQUENCE_CHECKER(sequence_checker_);
};

PageLiveStateDecoratorHelper::PageLiveStateDecoratorHelper() {
  PerformanceManager::AddObserver(this);

  MediaCaptureDevicesDispatcher::GetInstance()
      ->GetMediaStreamCaptureIndicator()
      ->AddObserver(this);
}

PageLiveStateDecoratorHelper::~PageLiveStateDecoratorHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
