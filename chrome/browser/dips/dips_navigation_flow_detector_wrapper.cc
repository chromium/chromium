// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_navigation_flow_detector_wrapper.h"

#include "chrome/browser/dips/dips_navigation_flow_detector.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"

DipsNavigationFlowDetectorWrapper::DipsNavigationFlowDetectorWrapper(
    tabs::TabInterface& tab)
    : tab_(&tab) {
  DipsNavigationFlowDetector::MaybeCreateForWebContents(tab_->GetContents());
  tab_subscriptions_.push_back(
      tab_->RegisterWillDiscardContents(base::BindRepeating(
          &DipsNavigationFlowDetectorWrapper::WillDiscardContents,
          weak_factory_.GetWeakPtr())));
}

DipsNavigationFlowDetectorWrapper::~DipsNavigationFlowDetectorWrapper() {
  tab_->GetContents()->RemoveUserData(
      DipsNavigationFlowDetector::UserDataKey());
}

DipsNavigationFlowDetector* DipsNavigationFlowDetectorWrapper::GetDetector() {
  return DipsNavigationFlowDetector::FromWebContents(tab_->GetContents());
}

void DipsNavigationFlowDetectorWrapper::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  old_contents->RemoveUserData(DipsNavigationFlowDetector::UserDataKey());
  DipsNavigationFlowDetector::MaybeCreateForWebContents(new_contents);
}
