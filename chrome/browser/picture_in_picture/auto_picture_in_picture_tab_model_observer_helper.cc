// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_model_observer_helper.h"

#include "base/notimplemented.h"
#include "chrome/browser/android/tab_android.h"  // nogncheck crbug.com/413572035
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"

// static
std::unique_ptr<AutoPictureInPictureTabObserverHelperBase>
AutoPictureInPictureTabObserverHelperBase::Create(
    content::WebContents* web_contents,
    ActivatedChangedCallback callback) {
  return std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
      web_contents, std::move(callback));
}

AutoPictureInPictureTabModelObserverHelper::
    AutoPictureInPictureTabModelObserverHelper(
        content::WebContents* web_contents,
        ActivatedChangedCallback callback)
    : AutoPictureInPictureTabObserverHelperBase(web_contents,
                                                std::move(callback)) {}

AutoPictureInPictureTabModelObserverHelper::
    ~AutoPictureInPictureTabModelObserverHelper() {
  StopObserving();
}

void AutoPictureInPictureTabModelObserverHelper::StartObserving() {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
}

void AutoPictureInPictureTabModelObserverHelper::StopObserving() {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
}

content::WebContents*
AutoPictureInPictureTabModelObserverHelper::GetActiveWebContents() const {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
  return nullptr;
}

void AutoPictureInPictureTabModelObserverHelper::DidSelectTab(
    TabAndroid* tab,
    TabModel::TabSelectionType type) {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
}

void AutoPictureInPictureTabModelObserverHelper::TabRemoved(TabAndroid* tab) {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
}

void AutoPictureInPictureTabModelObserverHelper::OnTabModelAdded(
    TabModel* model) {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
}

void AutoPictureInPictureTabModelObserverHelper::OnTabModelRemoved(
    TabModel* model) {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
}

void AutoPictureInPictureTabModelObserverHelper::UpdateIsTabActivated() {
  NOTIMPLEMENTED();
  // TODO(crbug.com/421608904): add implementation
}
