// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/new_navigation_observer.h"

#include "content/public/browser/navigation_handle.h"

NewNavigationObserver* NewNavigationObserver::GetInstance() {
  static base::NoDestructor<NewNavigationObserver> instance;
  return instance.get();
}

NewNavigationObserver::NewNavigationObserver() = default;

NewNavigationObserver::~NewNavigationObserver() = default;

void NewNavigationObserver::StartObserving(content::WebContents* web_contents) {
  if (web_contents) {
    content::WebContents* main_web_contents =
        web_contents->GetOutermostWebContents();
    navigation_observers_[main_web_contents] =
        std::make_unique<NewNavigationObserver::DownloadNavigationObserver>(
            main_web_contents);
  }
}

void NewNavigationObserver::StopObserving(content::WebContents* web_contents) {
  if (web_contents) {
    navigation_observers_.erase(web_contents->GetOutermostWebContents());
  }
}

bool NewNavigationObserver::HasNewNavigation(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  content::WebContents* main_web_contents =
      web_contents->GetOutermostWebContents();
  if (navigation_observers_.find(main_web_contents) !=
      navigation_observers_.end()) {
    return navigation_observers_[main_web_contents]->has_new_navigation();
  }
  return false;
}

NewNavigationObserver::DownloadNavigationObserver::DownloadNavigationObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

NewNavigationObserver::DownloadNavigationObserver::
    ~DownloadNavigationObserver() = default;

void NewNavigationObserver::DownloadNavigationObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame()) {
    has_new_navigation_ = true;
  }
}

void NewNavigationObserver::DownloadNavigationObserver::WebContentsDestroyed() {
  NewNavigationObserver::GetInstance()->StopObserving(web_contents());
}
