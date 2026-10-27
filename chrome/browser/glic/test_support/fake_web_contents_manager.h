// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_WEB_CONTENTS_MANAGER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "content/public/browser/visibility.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

class Host;
class ScopedModalDialogManagerDelegate;

class FakeWebContentsManager : public GlicWebContentsManager {
 public:
  explicit FakeWebContentsManager(content::WebContents* web_contents = nullptr);
  ~FakeWebContentsManager() override;

  FakeWebContentsManager(const FakeWebContentsManager&) = delete;
  FakeWebContentsManager& operator=(const FakeWebContentsManager&) = delete;

  // GlicWebContentsManager implementation:
  void AttachToHost(Host* host) override;
  void AttachModalDialogManagerDelegate(
      ScopedModalDialogManagerDelegate& delegate) override;
  void SetVisibility(content::Visibility visibility) override;
  content::WebContents* active_web_contents() const override;
  content::WebContents* guest_contents() const override;
  void OnActuatingChanged(bool actuating) override;
  void OnTaskTabsVisibilityChanged(bool has_visible_tab) override;
  base::CallbackListSubscription RegisterWebContentsChangedCallback(
      WebContentsChangedCallback callback) override;
  GlicWebClientManager& web_client_manager() override;
  bool ShouldReloadOnShow() const override;
  bool IsCrashed() const override;
  void SetErrorCallback(base::RepeatingClosure callback) override;

  // Test helpers:
  void TriggerErrorCallback();
  void set_web_contents(content::WebContents* web_contents) {
    web_contents_ = web_contents;
  }
  void set_should_reload_on_show(bool reload) {
    should_reload_on_show_ = reload;
  }
  Host* host() const { return host_; }
  content::Visibility visibility() const { return visibility_; }
  bool is_actuating() const { return is_actuating_; }
  bool has_visible_task_tabs() const { return has_visible_task_tabs_; }

 private:
  GlicWebClientManager web_client_manager_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<Host> host_ = nullptr;
  content::Visibility visibility_ = content::Visibility::HIDDEN;
  bool is_actuating_ = false;
  bool has_visible_task_tabs_ = false;
  bool should_reload_on_show_ = false;
  base::RepeatingClosure error_callback_;
  base::RepeatingCallbackList<void(content::WebContents*)>
      web_contents_changed_callbacks_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_WEB_CONTENTS_MANAGER_H_
