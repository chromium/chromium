// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/test_app_banner_manager_desktop.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/webapps/webapps_client_desktop.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace webapps {

TestAppBannerManagerDesktop::TestAppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManagerDesktop(web_contents) {
  // Ensure no real instance exists. This must be the only instance to avoid
  // observers of AppBannerManager left observing the wrong one.
  DCHECK_EQ(AppBannerManagerDesktop::FromWebContents(web_contents), nullptr);
  AddObserver(this);
}

TestAppBannerManagerDesktop::~TestAppBannerManagerDesktop() {
  RemoveObserver(this);
}

static std::unique_ptr<AppBannerManagerDesktop> CreateTestAppBannerManager(
    content::WebContents* web_contents) {
  return std::make_unique<TestAppBannerManagerDesktop>(web_contents);
}

void TestAppBannerManagerDesktop::SetUp() {
  AppBannerManagerDesktop::override_app_banner_manager_desktop_for_testing_ =
      CreateTestAppBannerManager;
  WebappsClientDesktop::CreateSingleton();
}

TestAppBannerManagerDesktop* TestAppBannerManagerDesktop::FromWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // Must call TestAppBannerManagerDesktop::SetUp() first.
  DCHECK_EQ(
      AppBannerManagerDesktop::override_app_banner_manager_desktop_for_testing_,
      CreateTestAppBannerManager);
  auto* manager = AppBannerManagerDesktop::FromWebContents(web_contents);
  DCHECK(manager);
  DCHECK(manager->AsTestAppBannerManagerDesktopForTesting());
  return manager->AsTestAppBannerManagerDesktopForTesting();
}

void TestAppBannerManagerDesktop::WaitForInstallableCheckTearDown() {
  base::RunLoop run_loop;
  tear_down_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool TestAppBannerManagerDesktop::WaitForInstallableCheck() {
  if (installable_check_in_progress_) {
    base::RunLoop run_loop;
    installable_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  CHECK(!installable_check_in_progress_);
  return IsPromotableWebApp();
}

void TestAppBannerManagerDesktop::SetBannerPromptReplyCallback(
    base::OnceClosure on_banner_prompt_reply) {
  on_banner_prompt_reply_ = std::move(on_banner_prompt_reply);
}

void TestAppBannerManagerDesktop::SetCompleteCallback(
    base::OnceClosure on_complete) {
  on_complete_ = std::move(on_complete);
}

AppBannerManager::State TestAppBannerManagerDesktop::state() {
  return AppBannerManager::state();
}

void TestAppBannerManagerDesktop::AwaitAppInstall() {
  base::RunLoop loop;
  on_install_ = loop.QuitClosure();
  loop.Run();
}

void TestAppBannerManagerDesktop::OnWebAppInstallableCheckedNoErrors(
    const ManifestId&) {
  RunInstallableQuitClosureIfNeeded();
}

void TestAppBannerManagerDesktop::ResetCurrentPageData() {
  debug_log_.Append("ResetCurrentPageData");
  AppBannerManagerDesktop::ResetCurrentPageData();
  installable_check_in_progress_ = true;
  if (tear_down_quit_closure_)
    std::move(tear_down_quit_closure_).Run();
}

TestAppBannerManagerDesktop*
TestAppBannerManagerDesktop::AsTestAppBannerManagerDesktopForTesting() {
  return this;
}

void TestAppBannerManagerDesktop::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  debug_log_.Append(base::StrCat({"DidFinishLoad ", validated_url.spec()}));
  // This mirrors AppBannerManager::UrlType::kInvalidPrimaryFrameUrl in
  // AppBannerManager::GetUrlType()
  if (content::HasWebUIScheme(validated_url) &&
      (validated_url.GetHost() !=
       password_manager::kChromeUIPasswordManagerHost)) {
    RunInstallableQuitClosureIfNeeded();
    return;
  }

  AppBannerManagerDesktop::DidFinishLoad(render_frame_host, validated_url);
}

void TestAppBannerManagerDesktop::RunInstallableQuitClosureIfNeeded() {
  debug_log_.Append("RunInstallableQuitClosureIfNeeded");
  if (installable_quit_closure_) {
    CHECK(installable_check_in_progress_);
    std::move(installable_quit_closure_).Run();
  }
  installable_check_in_progress_ = false;
}

void TestAppBannerManagerDesktop::WillFetchManifest() {
  debug_log_.Append("WillFetchManifest");
  installable_check_in_progress_ = true;
}

void TestAppBannerManagerDesktop::OnInstall() {
  if (on_install_) {
    std::move(on_install_).Run();
  }
}

void TestAppBannerManagerDesktop::OnBannerPromptReply() {
  CHECK(!installable_check_in_progress_);
  if (on_banner_prompt_reply_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_banner_prompt_reply_));
  }
}

void TestAppBannerManagerDesktop::OnComplete() {
  RunInstallableQuitClosureIfNeeded();
  if (on_complete_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_complete_));
  }
}

}  // namespace webapps
