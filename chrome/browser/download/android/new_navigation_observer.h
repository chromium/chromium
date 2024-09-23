// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_NEW_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_NEW_NAVIGATION_OBSERVER_H_

#include <map>

#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

// Singleton class for observing whether a new navigation has started in a list
// of WebContents.
class NewNavigationObserver {
 public:
  static NewNavigationObserver* GetInstance();

  NewNavigationObserver(const NewNavigationObserver&) = delete;
  NewNavigationObserver& operator=(const NewNavigationObserver&) = delete;

  // Called to start a new observation for a WebContents.
  void StartObserving(content::WebContents* web_contents);

  // Called to stop observing a WebContents.
  void StopObserving(content::WebContents* web_contents);

  // Called to check whether there are any new navigations on the WebContents
  // since observation.
  bool HasNewNavigation(content::WebContents* web_contents);

 private:
  friend base::NoDestructor<NewNavigationObserver>;

  NewNavigationObserver();
  ~NewNavigationObserver();

  class DownloadNavigationObserver : public content::WebContentsObserver {
   public:
    explicit DownloadNavigationObserver(content::WebContents* web_contents);
    ~DownloadNavigationObserver() override;
    DownloadNavigationObserver(const DownloadNavigationObserver&) = delete;
    DownloadNavigationObserver& operator=(const DownloadNavigationObserver&) =
        delete;

    // content::WebContentsObserver overrides.
    void DidStartNavigation(
        content::NavigationHandle* navigation_handle) override;
    void WebContentsDestroyed() override;

    bool has_new_navigation() { return has_new_navigation_; }

   private:
    bool has_new_navigation_ = false;
  };

  std::map<content::WebContents*, std::unique_ptr<DownloadNavigationObserver>>
      navigation_observers_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_NEW_NAVIGATION_OBSERVER_H_
