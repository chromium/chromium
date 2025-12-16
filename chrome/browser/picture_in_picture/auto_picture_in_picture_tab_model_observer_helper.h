// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_MODEL_OBSERVER_HELPER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_MODEL_OBSERVER_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_observer_helper_base.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

class TabModel;
class TabAndroid;

namespace content {
class WebContents;
}  // namespace content

// Android implementation of AutoPictureInPictureTabObserverHelperBase.
// Observes a TabModel to detect when the associated WebContents becomes active
// or inactive.
class AutoPictureInPictureTabModelObserverHelper final
    : public AutoPictureInPictureTabObserverHelperBase,
      public TabModelObserver,
      public TabModelListObserver {
 public:
  AutoPictureInPictureTabModelObserverHelper(content::WebContents* web_contents,
                                             ActivatedChangedCallback callback);
  ~AutoPictureInPictureTabModelObserverHelper() override;

  // AutoPictureInPictureTabObserverHelperBase:
  void StartObserving() override;
  void StopObserving() override;
  content::WebContents* GetActiveWebContents() const override;

  // TabModelObserver:
  void DidSelectTab(TabAndroid* tab, TabModel::TabSelectionType type) override;
  void TabRemoved(TabAndroid* tab) override;
  void WillCloseTab(TabAndroid* tab) override;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* model) override;
  void OnTabModelRemoved(TabModel* model) override;

 private:
  // Find the correct tab model to observe.
  void ReevaluateObservedModelAndState();

  // Updates `is_tab_activated_` based on the current model state and runs the
  // callback if it changed.
  void UpdateIsTabActivated();

  raw_ptr<TabModel> observed_tab_model_ = nullptr;
  bool is_tab_activated_ = false;
  bool is_observing_ = false;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_TAB_MODEL_OBSERVER_HELPER_H_
