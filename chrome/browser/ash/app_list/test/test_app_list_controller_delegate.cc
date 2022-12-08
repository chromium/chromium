// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"

#include <utility>

#include "ui/display/types/display_constants.h"
#include "ui/gfx/image/image_skia.h"

namespace test {

TestAppListControllerDelegate::TestAppListControllerDelegate() {
}

TestAppListControllerDelegate::~TestAppListControllerDelegate() {
}

int64_t TestAppListControllerDelegate::GetAppListDisplayId() {
  return display::kInvalidDisplayId;
}

void TestAppListControllerDelegate::DismissView() {
  did_dismiss_view_ = true;
}

aura::Window* TestAppListControllerDelegate::GetAppListWindow() {
  return nullptr;
}

bool TestAppListControllerDelegate::IsAppPinned(const std::string& app_id) {
  return false;
}

void TestAppListControllerDelegate::PinApp(const std::string& app_id) {
}

void TestAppListControllerDelegate::UnpinApp(const std::string& app_id) {
}

AppListControllerDelegate::Pinnable TestAppListControllerDelegate::GetPinnable(
    const std::string& app_id) {
  return NO_PIN;
}

bool TestAppListControllerDelegate::IsAppOpen(const std::string& app_id) const {
  return false;
}

void TestAppListControllerDelegate::DoShowAppInfoFlow(
    Profile* profile,
    const std::string& extension_id) {
}

void TestAppListControllerDelegate::CreateNewWindow(
    bool incognito,
    bool should_trigger_session_restore) {}

void TestAppListControllerDelegate::OpenURL(Profile* profile,
                                            const GURL& url,
                                            ui::PageTransition transition,
                                            WindowOpenDisposition deposition) {
  last_opened_url_ = url;
}

void TestAppListControllerDelegate::Reset() {
  did_dismiss_view_ = false;
  last_opened_url_ = GURL();
}

}  // namespace test
