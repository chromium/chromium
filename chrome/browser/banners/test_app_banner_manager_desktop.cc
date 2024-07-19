// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/test_app_banner_manager_desktop.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/webapps/webapps_client_desktop.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"

namespace webapps {

TestAppBannerManagerDesktop::TestAppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManagerDesktop(web_contents) {
  // Ensure no real instance exists. This must be the only instance to avoid
  // observers of AppBannerManager left observing the wrong one.
  DCHECK_EQ(AppBannerManagerDesktop::FromWebContents(web_contents), nullptr);
}

TestAppBannerManagerDesktop::~TestAppBannerManagerDesktop() = default;

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
  if (!installable_.has_value()) {
    base::RunLoop run_loop;
    installable_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  return *installable_ && IsPromotableWebApp();
}

void TestAppBannerManagerDesktop::PrepareDone(base::OnceClosure on_done) {
  on_done_ = std::move(on_done);
}

AppBannerManager::State TestAppBannerManagerDesktop::state() {
  return AppBannerManager::state();
}

void TestAppBannerManagerDesktop::AwaitAppInstall() {
  base::RunLoop loop;
  on_install_ = loop.QuitClosure();
  loop.Run();
}

void TestAppBannerManagerDesktop::OnDidGetManifest(
    const InstallableData& result) {
  debug_log_.Append("OnDidGetManifest");
  AppBannerManagerDesktop::OnDidGetManifest(result);

  // The manifest URL changing in the middle of a pipeline doesn't always mean
  // the page data will be reset. To ensure that installable_ isn't accidentally
  // set twice, reset it here.
  if (base::Contains(result.errors,
                     InstallableStatusCode::MANIFEST_URL_CHANGED)) {
    installable_.reset();
  } else if (blink::IsEmptyManifest(*result.manifest)) {
    // AppBannerManagerDesktop does not call
    // |OnDidPerformInstallableWebAppCheck| to complete the installability check
    // in this case, instead it early exits with failure.
    SetInstallable(false);
  }
}
void TestAppBannerManagerDesktop::OnDidPerformInstallableWebAppCheck(
    const InstallableData& result) {
  debug_log_.Append("OnDidPerformInstallableWebAppCheck");
  AppBannerManagerDesktop::OnDidPerformInstallableWebAppCheck(result);
  SetInstallable(result.errors.empty());
}

void TestAppBannerManagerDesktop::ResetCurrentPageData() {
  debug_log_.Append("ResetCurrentPageData");
  AppBannerManagerDesktop::ResetCurrentPageData();
  installable_.reset();
  if (tear_down_quit_closure_)
    std::move(tear_down_quit_closure_).Run();
}

void TestAppBannerManagerDesktop::RecheckInstallabilityForLoadedPage() {
  debug_log_.Append("RecheckInstallabilityForLoadedPage");
  installable_.reset();
  AppBannerManagerDesktop::RecheckInstallabilityForLoadedPage();
}

TestAppBannerManagerDesktop*
TestAppBannerManagerDesktop::AsTestAppBannerManagerDesktopForTesting() {
  return this;
}

void TestAppBannerManagerDesktop::OnInstall(
    blink::mojom::DisplayMode display,
    bool set_current_web_app_not_installable) {
  AppBannerManager::OnInstall(display, set_current_web_app_not_installable);
  if (on_install_)
    std::move(on_install_).Run();
}

void TestAppBannerManagerDesktop::DidFinishCreatingWebApp(
    const webapps::ManifestId& manifest_id,
    base::WeakPtr<AppBannerManagerDesktop> is_navigation_current,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  AppBannerManagerDesktop::DidFinishCreatingWebApp(
      manifest_id, is_navigation_current, app_id, code);
  OnFinished();
}

void TestAppBannerManagerDesktop::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  debug_log_.Append(base::StrCat({"DidFinishLoad ", validated_url.spec()}));
  UrlType url_type = GetUrlType(render_frame_host, validated_url);
  if (url_type == AppBannerManager::UrlType::kInvalidPrimaryFrameUrl) {
    SetInstallable(false);
    return;
  }

  AppBannerManagerDesktop::DidFinishLoad(render_frame_host, validated_url);
}

void TestAppBannerManagerDesktop::UpdateState(AppBannerManager::State state) {
  debug_log_.Append(
      base::StringPrintf("State updated to %d", static_cast<int>(state)));
  AppBannerManager::UpdateState(state);

  if (state == AppBannerManager::State::PENDING_ENGAGEMENT ||
      state == AppBannerManager::State::PENDING_PROMPT_CANCELED ||
      state == AppBannerManager::State::PENDING_PROMPT_NOT_CANCELED ||
      state == AppBannerManager::State::COMPLETE) {
    OnFinished();
  }
}

void TestAppBannerManagerDesktop::SetInstallable(bool installable) {
  debug_log_.Append(base::StringPrintf("SetInstallable(%d)", installable));
  DCHECK(!installable_.has_value() || installable_ == installable)
      << "Cannot set installable to " << installable << ", already set to "
      << installable_.value() << ". Debug log:\n"
      << debug_log_.DebugString();
  installable_ = installable;
  if (installable_quit_closure_)
    std::move(installable_quit_closure_).Run();
}

void TestAppBannerManagerDesktop::OnFinished() {
  if (on_done_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_done_));
  }
}

}  // namespace webapps
