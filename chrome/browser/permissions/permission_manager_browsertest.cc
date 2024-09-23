// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace {

permissions::PermissionManager::PermissionContextMap CreatePermissionContexts(
    Profile* profile) {
  permissions::PermissionManager::PermissionContextMap permission_contexts;
  permission_contexts[ContentSettingsType::GEOLOCATION] =
      std::make_unique<permissions::GeolocationPermissionContext>(
          profile,
          std::make_unique<GeolocationPermissionContextDelegate>(profile));
  return permission_contexts;
}

}  // namespace

// PermissionManager subclass that enables the test below to deterministically
// wait until there is a permission status subscription from a service worker.
// Deleting the off-the-record profile under these circumstances would
// previously have resulted in a crash.
class SubscriptionInterceptingPermissionManager
    : public permissions::PermissionManager {
 public:
  explicit SubscriptionInterceptingPermissionManager(Profile* profile)
      : permissions::PermissionManager(profile,
                                       CreatePermissionContexts(profile)) {}

  ~SubscriptionInterceptingPermissionManager() override = default;

  void SetSubscribeCallback(base::RepeatingClosure callback) {
    callback_ = std::move(callback);
  }

  void OnPermissionStatusChangeSubscriptionAdded(
      content::PermissionController::SubscriptionId subscription_id) override {
    permissions::PermissionManager::OnPermissionStatusChangeSubscriptionAdded(
        subscription_id);
    std::move(callback_).Run();
  }

 private:
  base::RepeatingClosure callback_;
};

class PermissionManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionManagerBrowserTest() = default;

  PermissionManagerBrowserTest(const PermissionManagerBrowserTest&) = delete;
  PermissionManagerBrowserTest& operator=(const PermissionManagerBrowserTest&) =
      delete;

  ~PermissionManagerBrowserTest() override = default;

  static std::unique_ptr<KeyedService> CreateTestingPermissionManager(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<SubscriptionInterceptingPermissionManager>(profile);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    incognito_browser_ = CreateIncognitoBrowser();

    PermissionManagerFactory::GetInstance()->SetTestingFactory(
        incognito_browser_->profile(),
        base::BindRepeating(
            &PermissionManagerBrowserTest::CreateTestingPermissionManager));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Browser* incognito_browser() { return incognito_browser_; }

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> incognito_browser_ = nullptr;
};

// TODO(crbug.com/41485058): Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(PermissionManagerBrowserTest,
                       DISABLED_ServiceWorkerPermissionQueryIncognitoClose) {
  base::RunLoop run_loop;
  permissions::PermissionManager* pm =
      PermissionManagerFactory::GetForProfile(incognito_browser()->profile());
  static_cast<SubscriptionInterceptingPermissionManager*>(pm)
      ->SetSubscribeCallback(run_loop.QuitClosure());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser(),
      embedded_test_server()->GetURL(
          "/permissions/permissions_service_worker.html")));
  run_loop.Run();

  // TODO(crbug.com/40092556) : We are relying here on the test shuts down to
  // close the browser. We need to make the test more robust by closing the
  // browser explicitly.
}

// TODO(crbug.com/329645039): Re-enable this test once fixed
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_CHROMEOS_ASH) && !defined(NDEBUG)) || \
    (defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_CHROMEOS))
#define MAYBE_ServiceWorkerPermissionAfterRendererCrash \
  DISABLED_ServiceWorkerPermissionAfterRendererCrash
#else
#define MAYBE_ServiceWorkerPermissionAfterRendererCrash \
  ServiceWorkerPermissionAfterRendererCrash
#endif
IN_PROC_BROWSER_TEST_F(PermissionManagerBrowserTest,
                       MAYBE_ServiceWorkerPermissionAfterRendererCrash) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes_;

  content::RenderProcessHostWatcher crash_observer(
      incognito_browser()->tab_strip_model()->GetActiveWebContents(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  incognito_browser()->OpenURL(
      content::OpenURLParams(
          GURL(blink::kChromeUICrashURL), content::Referrer(),
          WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  crash_observer.Wait();

  base::RunLoop run_loop;
  auto* pm = static_cast<SubscriptionInterceptingPermissionManager*>(
      PermissionManagerFactory::GetForProfile(incognito_browser()->profile()));
  pm->SetSubscribeCallback(run_loop.QuitClosure());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito_browser(),
      embedded_test_server()->GetURL(
          "/permissions/permissions_service_worker.html")));
  run_loop.Run();
}
