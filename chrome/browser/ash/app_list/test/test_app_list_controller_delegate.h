// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_ASH_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_DELEGATE_H_

#include <string>

#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"

namespace test {

class TestAppListControllerDelegate : public AppListControllerDelegate {
 public:
  TestAppListControllerDelegate();
  ~TestAppListControllerDelegate() override;

  int64_t GetAppListDisplayId() override;
  void DismissView() override;
  aura::Window* GetAppListWindow() override;
  bool IsAppPinned(const std::string& app_id) override;
  void PinApp(const std::string& app_id) override;
  void UnpinApp(const std::string& app_id) override;
  Pinnable GetPinnable(const std::string& app_id) override;
  bool IsAppOpen(const std::string& app_id) const override;
  void DoShowAppInfoFlow(Profile* profile,
                         const std::string& extension_id) override;
  void CreateNewWindow(bool incognito,
                       bool should_trigger_session_restore) override;
  void OpenURL(Profile* profile,
               const GURL& url,
               ui::PageTransition transition,
               WindowOpenDisposition deposition) override;

  void Reset();

  bool did_dismiss_view() const { return did_dismiss_view_; }
  const GURL& last_opened_url() const { return last_opened_url_; }

 private:
  bool did_dismiss_view_ = false;
  GURL last_opened_url_;
};

}  // namespace test

#endif  // CHROME_BROWSER_ASH_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_DELEGATE_H_
