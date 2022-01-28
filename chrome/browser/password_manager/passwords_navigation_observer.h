// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORDS_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORDS_NAVIGATION_OBSERVER_H_

#include <string>

#include "base/run_loop.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"

class PasswordsNavigationObserver : public content::WebContentsObserver {
 public:
  explicit PasswordsNavigationObserver(content::WebContents* web_contents);

  PasswordsNavigationObserver(const PasswordsNavigationObserver&) = delete;
  PasswordsNavigationObserver& operator=(const PasswordsNavigationObserver&) =
      delete;

  ~PasswordsNavigationObserver() override;

  // Normally Wait() will not return until a main frame navigation occurs.
  // If a path is set, Wait() will return after this path has been seen,
  // regardless of the frame that navigated. Useful for multi-frame pages.
  void SetPathToWaitFor(const std::string& path) { wait_for_path_ = path; }

  // Normally Wait() will not return until a main frame navigation occurs.
  // If quit_on_entry_committed is true Wait() will return on EntryCommitted.
  void set_quit_on_entry_committed(bool quit_on_entry_committed) {
    quit_on_entry_committed_ = quit_on_entry_committed;
  }

  // Wait for navigation to succeed.
  void Wait();

  // Returns the RenderFrameHost that navigated.
  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  std::string wait_for_path_;
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  bool quit_on_entry_committed_ = false;
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORDS_NAVIGATION_OBSERVER_H_
