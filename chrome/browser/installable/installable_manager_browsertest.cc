// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_data.h"
#include "components/webapps/browser/installable/installable_manager.h"

#include <memory>
#include <string>
#include <tuple>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace webapps {

namespace {

const char kInsecureOrigin[] = "http://www.google.com";
const char kOtherInsecureOrigin[] = "http://maps.google.com";
const char kUnsafeSecureOriginFlag[] =
    "unsafely-treat-insecure-origin-as-secure";

InstallableParams GetManifestParams() {
  InstallableParams params;
  params.check_eligibility = true;
  params.wait_for_worker = true;
  return params;
}

InstallableParams GetWebAppParams() {
  InstallableParams params = GetManifestParams();
  params.valid_manifest = true;
  params.has_worker = true;
  params.valid_primary_icon = true;
  params.wait_for_worker = true;
  return params;
}

InstallableParams GetPrimaryIconParams() {
  InstallableParams params = GetManifestParams();
  params.valid_primary_icon = true;
  params.wait_for_worker = true;
  return params;
}

InstallableParams GetPrimaryIconAndSplashIconParams() {
  InstallableParams params = GetManifestParams();
  params.valid_primary_icon = true;
  params.valid_splash_icon = true;
  params.wait_for_worker = true;
  return params;
}

InstallableParams GetPrimaryIconPreferMaskableParams() {
  InstallableParams params = GetManifestParams();
  params.valid_primary_icon = true;
  params.prefer_maskable_icon = true;
  params.wait_for_worker = true;
  return params;
}

InstallableParams GetPreferMaskablePrimaryAndSplashIconParams() {
  InstallableParams params = GetManifestParams();
  params.valid_primary_icon = true;
  params.prefer_maskable_icon = true;
  params.valid_splash_icon = true;
  params.wait_for_worker = true;
  return params;
}

}  // anonymous namespace

// Used only for testing pages with no service workers. This class will dispatch
// a RunLoop::QuitClosure when it begins waiting for a service worker to be
// registered.
class LazyWorkerInstallableManager : public InstallableManager {
 public:
  LazyWorkerInstallableManager(content::WebContents* web_contents,
                               base::OnceClosure quit_closure)
      : InstallableManager(web_contents),
        quit_closure_(std::move(quit_closure)) {}
  ~LazyWorkerInstallableManager() override = default;

 protected:
  void OnWaitingForServiceWorker() override { std::move(quit_closure_).Run(); }

 private:
  base::OnceClosure quit_closure_;
};

// Used only for testing pages where the manifest URL is changed. This class
// will dispatch a RunLoop::QuitClosure when internal state is reset.
class ResetDataInstallableManager : public InstallableManager {
 public:
  explicit ResetDataInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}
  ~ResetDataInstallableManager() override {}

  void SetQuitClosure(base::RepeatingClosure quit_closure) {
    quit_closure_ = quit_closure;
  }

  bool GetOnResetData() { return is_reset_data_; }
  void ClearOnResetData() { is_reset_data_ = false; }

 protected:
  void OnResetData() override {
    is_reset_data_ = true;
    if (quit_closure_)
      quit_closure_.Run();
  }

 private:
  base::RepeatingClosure quit_closure_;
  bool is_reset_data_ = false;
};

class CallbackTester {
 public:
  explicit CallbackTester(base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}

  void OnDidFinishInstallableCheck(const InstallableData& data) {
    errors_ = data.errors;
    manifest_url_ = *data.manifest_url;
    manifest_ = data.manifest->Clone();
    primary_icon_url_ = *data.primary_icon_url;
    if (data.primary_icon)
      primary_icon_ = std::make_unique<SkBitmap>(*data.primary_icon);
    has_maskable_primary_icon_ = data.has_maskable_primary_icon;
    splash_icon_url_ = *data.splash_icon_url;
    if (data.splash_icon)
      splash_icon_ = std::make_unique<SkBitmap>(*data.splash_icon);
    has_maskable_splash_icon_ = data.has_maskable_splash_icon;
    valid_manifest_ = data.valid_manifest;
    worker_check_passed_ = data.worker_check_passed;
    screenshots_ = *data.screenshots;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                             quit_closure_);
  }

  const std::vector<InstallableStatusCode>& errors() const { return errors_; }
  const GURL& manifest_url() const { return manifest_url_; }
  const blink::mojom::Manifest& manifest() const {
    DCHECK(manifest_);
    return *manifest_;
  }
  const GURL& primary_icon_url() const { return primary_icon_url_; }
  const SkBitmap* primary_icon() const { return primary_icon_.get(); }
  bool has_maskable_primary_icon() const { return has_maskable_primary_icon_; }
  const GURL& splash_icon_url() const { return splash_icon_url_; }
  bool has_maskable_splash_icon() const { return has_maskable_splash_icon_; }
  const SkBitmap* splash_icon() const { return splash_icon_.get(); }
  const std::vector<Screenshot>& screenshots() const { return screenshots_; }
  bool valid_manifest() const { return valid_manifest_; }
  bool worker_check_passed() const { return worker_check_passed_; }

 private:
  base::RepeatingClosure quit_closure_;
  std::vector<InstallableStatusCode> errors_;
  GURL manifest_url_;
  blink::mojom::ManifestPtr manifest_ = blink::mojom::Manifest::New();
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  bool has_maskable_primary_icon_;
  GURL splash_icon_url_;
  std::unique_ptr<SkBitmap> splash_icon_;
  std::vector<Screenshot> screenshots_;
  bool has_maskable_splash_icon_;
  bool valid_manifest_;
  bool worker_check_passed_;
};

class NestedCallbackTester {
 public:
  NestedCallbackTester(InstallableManager* manager,
                       const InstallableParams& params,
                       base::OnceClosure quit_closure)
      : manager_(manager),
        params_(params),
        quit_closure_(std::move(quit_closure)) {}

  void Run() {
    manager_->GetData(
        params_, base::BindOnce(&NestedCallbackTester::OnDidFinishFirstCheck,
                                base::Unretained(this)));
  }

  void OnDidFinishFirstCheck(const InstallableData& data) {
    errors_ = data.errors;
    manifest_url_ = *data.manifest_url;
    manifest_ = data.manifest->Clone();
    primary_icon_url_ = *data.primary_icon_url;
    if (data.primary_icon)
      primary_icon_ = std::make_unique<SkBitmap>(*data.primary_icon);
    valid_manifest_ = data.valid_manifest;
    worker_check_passed_ = data.worker_check_passed;

    manager_->GetData(
        params_, base::BindOnce(&NestedCallbackTester::OnDidFinishSecondCheck,
                                base::Unretained(this)));
  }

  void OnDidFinishSecondCheck(const InstallableData& data) {
    EXPECT_EQ(errors_, data.errors);
    EXPECT_EQ(manifest_url_, *data.manifest_url);
    EXPECT_EQ(primary_icon_url_, *data.primary_icon_url);
    EXPECT_EQ(primary_icon_.get(), data.primary_icon);
    EXPECT_EQ(valid_manifest_, data.valid_manifest);
    EXPECT_EQ(worker_check_passed_, data.worker_check_passed);
    EXPECT_EQ(blink::IsEmptyManifest(*manifest_),
              blink::IsEmptyManifest(*data.manifest));
    EXPECT_EQ(manifest_->start_url, data.manifest->start_url);
    EXPECT_EQ(manifest_->display, data.manifest->display);
    EXPECT_EQ(manifest_->name, data.manifest->name);
    EXPECT_EQ(manifest_->short_name, data.manifest->short_name);
    EXPECT_EQ(manifest_->display_override, data.manifest->display_override);

    std::move(quit_closure_).Run();
  }

 private:
  raw_ptr<InstallableManager> manager_;
  InstallableParams params_;
  base::OnceClosure quit_closure_;
  std::vector<InstallableStatusCode> errors_;
  GURL manifest_url_;
  blink::mojom::ManifestPtr manifest_;
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  bool valid_manifest_;
  bool worker_check_passed_;
};

class InstallableManagerBrowserTest : public InProcessBrowserTest {
 public:
  InstallableManagerBrowserTest()
      : disable_banner_trigger_(&test::g_disable_banner_triggering_for_testing,
                                true) {
    scoped_feature_list_.InitAndEnableFeature(
        webapps::features::kDesktopPWAsDetailedInstallDialog);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/banners");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns a test server URL to a page controlled by a service worker with
  // |manifest_url| injected as the manifest tag.
  std::string GetURLOfPageWithServiceWorkerAndManifest(
      const std::string& manifest_url) {
    return "/banners/manifest_test_page.html?manifest=" +
           embedded_test_server()->GetURL(manifest_url).spec();
  }

  void NavigateAndMaybeWaitForWorker(Browser* browser,
                                     const std::string& path,
                                     bool wait_for_worker = true) {
    GURL test_url = embedded_test_server()->GetURL(path);
    if (wait_for_worker) {
      web_app::ServiceWorkerRegistrationWaiter registration_waiter(
          browser->profile(), test_url);
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_url));
      registration_waiter.AwaitRegistration();
    } else {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, test_url));
    }
  }

  void NavigateAndRunInstallableManager(Browser* browser,
                                        CallbackTester* tester,
                                        const InstallableParams& params,
                                        const std::string& url,
                                        bool wait_for_worker = true) {
    NavigateAndMaybeWaitForWorker(browser, url, wait_for_worker);
    RunInstallableManager(browser, tester, params);
  }

  std::vector<content::InstallabilityError>
  NavigateAndGetAllInstallabilityErrors(Browser* browser,
                                        const std::string& url) {
    NavigateAndMaybeWaitForWorker(browser, url);
    return GetAllInstallabilityErrors(browser);
  }

  void RunInstallableManager(Browser* browser,
                             CallbackTester* tester,
                             const InstallableParams& params) {
    InstallableManager* manager = GetManager(browser);
    manager->GetData(
        params, base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                               base::Unretained(tester)));
  }

  InstallableManager* GetManager(Browser* browser) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    InstallableManager::CreateForWebContents(web_contents);
    InstallableManager* manager =
        InstallableManager::FromWebContents(web_contents);
    CHECK(manager);

    return manager;
  }

  std::vector<content::InstallabilityError> GetAllInstallabilityErrors(
      Browser* browser) {
    InstallableManager* manager = GetManager(browser);

    base::RunLoop run_loop;
    std::vector<content::InstallabilityError> result;

    manager->GetAllErrors(base::BindLambdaForTesting(
        [&](std::vector<content::InstallabilityError> installability_errors) {
          result = std::move(installability_errors);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

 private:
  // Disable the banners in the browser so it won't interfere with the test.
  base::AutoReset<bool> disable_banner_trigger_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

class InstallableManagerAllowlistOriginBrowserTest
    : public InstallableManagerBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(kUnsafeSecureOriginFlag, kInsecureOrigin);
  }
};

enum class CheckOfflineCapabilityMode { NONE = 0, WARN_ONLY = 1, ENFORCE = 2 };

class InstallableManagerOfflineCapabilityBrowserTest
    : public InstallableManagerBrowserTest,
      public testing::WithParamInterface<
          std::tuple<CheckOfflineCapabilityMode, bool>> {
 public:
  InstallableManagerOfflineCapabilityBrowserTest()
      : offline_capability_type_(std::get<0>(GetParam())),
        is_service_worker_offline_supported_(std::get<1>(GetParam())) {
    switch (offline_capability_type_) {
      case CheckOfflineCapabilityMode::NONE:
        scoped_feature_list_.InitAndDisableFeature(
            blink::features::kCheckOfflineCapability);
        break;
      case CheckOfflineCapabilityMode::WARN_ONLY:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            blink::features::kCheckOfflineCapability,
            {{"check_mode", "warn_only"}});
        break;
      case CheckOfflineCapabilityMode::ENFORCE:
        scoped_feature_list_.InitAndEnableFeatureWithParameters(
            blink::features::kCheckOfflineCapability,
            {{"check_mode", "enforce"}});
        break;
    }
  }
  ~InstallableManagerOfflineCapabilityBrowserTest() override = default;

  bool IsServiceWorkerOfflineSupported() {
    return is_service_worker_offline_supported_;
  }

  bool IsCheckOfflineCapableFeatureEnabled() {
    return offline_capability_type_ == CheckOfflineCapabilityMode::WARN_ONLY ||
           offline_capability_type_ == CheckOfflineCapabilityMode::ENFORCE;
  }

  // Check the result of `tester` depending on whether or not a service worker
  // supports offline pages and the CheckOfflineCapability feature.
  void CheckServiceWorkerForTester(CallbackTester* tester) {
    if (is_service_worker_offline_supported_ ||
        offline_capability_type_ == CheckOfflineCapabilityMode::NONE) {
      EXPECT_TRUE(tester->worker_check_passed());
      EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
    } else if (offline_capability_type_ ==
               CheckOfflineCapabilityMode::WARN_ONLY) {
      EXPECT_TRUE(tester->worker_check_passed());
      EXPECT_EQ(std::vector<InstallableStatusCode>{WARN_NOT_OFFLINE_CAPABLE},
                tester->errors());
    } else {
      EXPECT_FALSE(tester->worker_check_passed());
      EXPECT_EQ(std::vector<InstallableStatusCode>{NOT_OFFLINE_CAPABLE},
                tester->errors());
    }
  }

  // Check the result of `manager` depending on whether or not a service worker
  // supports offline pages and the CheckOfflineCapability feature.
  void CheckServiceWorkerForInstallableManager(InstallableManager* manager) {
    if (is_service_worker_offline_supported_ ||
        offline_capability_type_ == CheckOfflineCapabilityMode::NONE) {
      EXPECT_TRUE(manager->has_worker());
      EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
    } else if (offline_capability_type_ ==
               CheckOfflineCapabilityMode::WARN_ONLY) {
      EXPECT_TRUE(manager->has_worker());
      EXPECT_EQ(WARN_NOT_OFFLINE_CAPABLE, manager->worker_error());
    } else {
      EXPECT_FALSE(manager->has_worker());
      EXPECT_EQ(NOT_OFFLINE_CAPABLE, manager->worker_error());
    }
  }

  // Assume that the name of HTML files using a service worker with an empty
  // fetch event handler includes "_empty_fetch_handler" suffix.
  const std::string GetPath(std::string base) {
    if (is_service_worker_offline_supported_)
      return base + ".html";
    return base + "_empty_fetch_handler.html";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  const CheckOfflineCapabilityMode offline_capability_type_;
  const bool is_service_worker_offline_supported_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InstallableManagerOfflineCapabilityBrowserTest,
    testing::Combine(testing::Values(CheckOfflineCapabilityMode::NONE,
                                     CheckOfflineCapabilityMode::WARN_ONLY,
                                     CheckOfflineCapabilityMode::ENFORCE),
                     testing::Bool()));

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManagerBeginsInEmptyState) {
  // Ensure that the InstallableManager starts off with everything null.
  InstallableManager* manager = GetManager(browser());

  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_TRUE(manager->manifest_url().is_empty());
  EXPECT_TRUE(manager->icons_.empty());
  EXPECT_FALSE(manager->valid_manifest());
  EXPECT_FALSE(manager->has_worker());

  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, ManagerInIncognito) {
  // Ensure that the InstallableManager returns an error if called in an
  // incognito profile.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  InstallableManager* manager = GetManager(incognito_browser);

  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndMaybeWaitForWorker(incognito_browser,
                                "/banners/manifest_test_page.html");

  RunInstallableManager(incognito_browser, tester.get(), GetManifestParams());
  run_loop.Run();

  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_TRUE(manager->manifest_url().is_empty());
  EXPECT_TRUE(manager->icons_.empty());
  EXPECT_FALSE(manager->valid_manifest());
  EXPECT_FALSE(manager->has_worker());

  EXPECT_EQ(IN_INCOGNITO, manager->eligibility_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckNoManifest) {
  // Ensure that a page with no manifest returns the appropriate error and with
  // null fields for everything.
  base::HistogramTester histograms;
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  // Navigating resets histogram state, so do it before recording a histogram.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html")));
  RunInstallableManager(browser(), tester.get(), GetManifestParams());
  run_loop.Run();

  // If there is no manifest, everything should be empty.
  EXPECT_TRUE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_FALSE(tester->has_maskable_splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{NO_MANIFEST}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifest404) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(browser(), tester.get(), GetManifestParams(),
                                   GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/manifest_missing.json"));
  run_loop.Run();

  // The installable manager should return a manifest URL even if it 404s.
  // However, the check should fail with a ManifestEmpty error.
  EXPECT_TRUE(blink::IsEmptyManifest(tester->manifest()));

  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_FALSE(tester->has_maskable_splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{MANIFEST_EMPTY},
            tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestOnly) {
  // Verify that asking for just the manifest works as expected.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(browser(), tester.get(), GetManifestParams(),
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_FALSE(tester->has_maskable_splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckInstallableParamsDefaultConstructor) {
  // Verify that using InstallableParams' default constructor is equivalent to
  // just asking for the manifest alone.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params;
  NavigateAndRunInstallableManager(browser(), tester.get(), params,
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_FALSE(tester->has_maskable_splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestWithIconThatIsTooSmall) {
  // This page has a manifest with only a 48x48 icon which is too small to be
  // installable. Asking for a primary icon should fail with NO_ACCEPTABLE_ICON.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        browser(), tester.get(), GetPrimaryIconParams(),
        GetURLOfPageWithServiceWorkerAndManifest(
            "/banners/manifest_too_small_icon.json"));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_ACCEPTABLE_ICON},
              tester->errors());
  }

  // Ask for everything except splash icon. This should fail with
  // NO_ACCEPTABLE_ICON - the primary icon fetch has already failed, so that
  // cached error stops the installable check from being performed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(browser(), tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_FALSE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_ACCEPTABLE_ICON},
              tester->errors());
  }

  // Ask for a splash icon. This should fail to get a splash icon but not record
  // an error.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetPrimaryIconAndSplashIconParams();
    params.valid_primary_icon = false;
    RunInstallableManager(browser(), tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestWithOnlyRelatedApplications) {
  // This page has a manifest with only related applications specified. Asking
  // for just the manifest should succeed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(browser(), tester.get(),
                                     GetManifestParams(),
                                     GetURLOfPageWithServiceWorkerAndManifest(
                                         "/banners/play_app_manifest.json"));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Ask for a primary icon (but don't navigate). This should fail with
  // NO_ACCEPTABLE_ICON.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(browser(), tester.get(), GetPrimaryIconParams());
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_ACCEPTABLE_ICON},
              tester->errors());
  }

  // Ask for everything except splash icon. This should fail with
  // NO_ACCEPTABLE_ICON - the primary icon fetch has already failed, so that
  // cached error stops the installable check from being performed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    auto params = GetWebAppParams();
    params.valid_manifest = false;
    RunInstallableManager(browser(), tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_FALSE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_ACCEPTABLE_ICON},
              tester->errors());
  }

  // Do not ask for primary icon. This should fail with several validity
  // errors.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.valid_primary_icon = false;
    RunInstallableManager(browser(), tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_FALSE(tester->worker_check_passed());
    EXPECT_EQ(
        std::vector<InstallableStatusCode>(
            {START_URL_NOT_VALID, MANIFEST_MISSING_NAME_OR_SHORT_NAME,
             MANIFEST_DISPLAY_NOT_SUPPORTED, MANIFEST_MISSING_SUITABLE_ICON}),
        tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestAndIcon) {
  // Add to homescreen checks for manifest + primary icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(browser(), tester.get(),
                                     GetPrimaryIconParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Add to homescreen checks for manifest + primary icon + splash icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(browser(), tester.get(),
                          GetPrimaryIconAndSplashIconParams());
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_FALSE(tester->has_maskable_splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Navigate to a page with a good maskable icon and a bad any
  // icon. The maskable icon is fetched for both primary and splash icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    NavigateAndRunInstallableManager(
        browser(), tester.get(), GetPreferMaskablePrimaryAndSplashIconParams(),
        GetURLOfPageWithServiceWorkerAndManifest(
            "/banners/manifest_bad_non_maskable_icon.json"));
    run_loop.Run();
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->has_maskable_primary_icon());
    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_TRUE(tester->has_maskable_splash_icon());

    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_P(InstallableManagerOfflineCapabilityBrowserTest,
                       CheckWebapp) {
  // Request everything except splash icon.
  {
    base::HistogramTester histograms;
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Navigating resets histogram state, so do it before recording a histogram.
    NavigateAndMaybeWaitForWorker(browser(),
                                  GetPath("/banners/manifest_test_page"));
    RunInstallableManager(browser(), tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->valid_manifest());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    CheckServiceWorkerForTester(tester.get());

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager(browser());

    EXPECT_FALSE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_EQ(1u, manager->icons_.size());
    EXPECT_FALSE(manager->valid_manifest());
    EXPECT_FALSE((
        manager->icon_url(InstallableManager::IconUsage::kPrimary).is_empty()));
    EXPECT_NE(nullptr,
              (manager->icon(InstallableManager::IconUsage::kPrimary)));
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED,
              (manager->icon_error(InstallableManager::IconUsage::kPrimary)));
    EXPECT_TRUE(!manager->task_queue_.HasCurrent());
    CheckServiceWorkerForInstallableManager(manager);
  }

  // Request everything except splash icon again without navigating away. This
  // should work fine.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    auto params = GetWebAppParams();
    // Make sure valid_manifest check is run.
    params.is_debug_mode = true;
    RunInstallableManager(browser(), tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->valid_manifest());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    CheckServiceWorkerForTester(tester.get());

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager(browser());

    EXPECT_FALSE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_EQ(1u, manager->icons_.size());
    EXPECT_FALSE(manager->valid_manifest());
    EXPECT_FALSE((
        manager->icon_url(InstallableManager::IconUsage::kPrimary).is_empty()));
    EXPECT_NE(nullptr,
              (manager->icon(InstallableManager::IconUsage::kPrimary)));
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED,
              (manager->icon_error(InstallableManager::IconUsage::kPrimary)));
    EXPECT_TRUE(!manager->task_queue_.HasCurrent());
    CheckServiceWorkerForInstallableManager(manager);
  }

  {
    // Check that a subsequent navigation resets state.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    InstallableManager* manager = GetManager(browser());

    EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_TRUE(manager->manifest_url().is_empty());
    EXPECT_FALSE(manager->valid_manifest());
    EXPECT_FALSE(manager->has_worker());
    EXPECT_TRUE(manager->icons_.empty());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
    EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckMaskableIcon) {
  // Checks that InstallableManager chooses the correct primary icon when the
  // manifest contains maskable icons.

  // Checks that if a MASKABLE icon is specified, it is used as primary icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        browser(), tester.get(), GetPreferMaskablePrimaryAndSplashIconParams(),
        GetURLOfPageWithServiceWorkerAndManifest(
            "/banners/manifest_maskable.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_TRUE(tester->has_maskable_splash_icon());

    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());

    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Checks that we don't pick a MASKABLE icon if it was not requested.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(browser(), tester.get(),
                                     GetPrimaryIconAndSplashIconParams(),
                                     GetURLOfPageWithServiceWorkerAndManifest(
                                         "/banners/manifest_maskable.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_FALSE(tester->has_maskable_splash_icon());

    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Checks that we fall back to using an ANY icon if a MASKABLE icon is
  // requested but not in the manifest.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        browser(), tester.get(), GetPreferMaskablePrimaryAndSplashIconParams(),
        "/banners/manifest_test_page.html");

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_FALSE(tester->has_maskable_splash_icon());

    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Checks that we fall back to using an ANY icon if a MASKABLE icon is
  // requested but the maskable icon is bad.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        browser(), tester.get(), GetPreferMaskablePrimaryAndSplashIconParams(),
        GetURLOfPageWithServiceWorkerAndManifest(
            "/banners/manifest_bad_maskable.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->has_maskable_primary_icon());

    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_FALSE(tester->has_maskable_splash_icon());

    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckNavigationWithoutRunning) {
  {
    // Expect the call to ManifestAndIconTimeout to kick off an installable
    // check and fail it on a not installable page.
    base::HistogramTester histograms;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL("/banners/no_manifest_test_page.html")));

    InstallableManager* manager = GetManager(browser());

    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Set up a GetData call which will not record an installable metric to
    // ensure we wait until the previous check has finished.
    manager->GetData(
        GetManifestParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  {
    // Expect the call to ManifestAndIconTimeout to kick off an installable
    // check and pass it on an installable page.
    base::HistogramTester histograms;
    NavigateAndMaybeWaitForWorker(browser(),
                                  "/banners/manifest_test_page.html");

    InstallableManager* manager = GetManager(browser());

    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Set up a GetData call which will not record an installable metric to
    // ensure we wait until the previous check has finished.
    manager->GetData(
        GetManifestParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckWebappInIframe) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(browser(), tester.get(), GetWebAppParams(),
                                   "/banners/iframe_test_page.html");
  run_loop.Run();

  // The installable manager should only retrieve items in the main frame;
  // everything should be empty here.
  EXPECT_TRUE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_FALSE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{NO_MANIFEST}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckPageWithManifestAndNoServiceWorker) {
  // Just fetch the manifest. This should have no error.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        browser(), tester.get(), GetManifestParams(),
        "/banners/manifest_no_service_worker.html", /*wait_for_worker=*/false);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }

  // Fetching the full criteria should fail if we don't wait for the worker.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.wait_for_worker = false;
    RunInstallableManager(browser(), tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->valid_manifest());
    EXPECT_FALSE(tester->worker_check_passed());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_MATCHING_SERVICE_WORKER},
              tester->errors());
  }
}

IN_PROC_BROWSER_TEST_P(InstallableManagerOfflineCapabilityBrowserTest,
                       CheckLazyServiceWorkerPassesWhenWaiting) {
  base::RunLoop tester_run_loop, sw_run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(tester_run_loop.QuitClosure()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto manager = std::make_unique<LazyWorkerInstallableManager>(
      web_contents, sw_run_loop.QuitClosure());

  {
    // Load a URL with no service worker.
    GURL test_url = embedded_test_server()->GetURL(
        "/banners/manifest_no_service_worker.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

    // Kick off fetching the data. This should block on waiting for a worker.
    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    sw_run_loop.Run();
  }

  // We should now be waiting for the service worker.
  EXPECT_TRUE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_FALSE(manager->manifest_url().is_empty());
  EXPECT_FALSE(manager->has_worker());
  EXPECT_EQ(1u, manager->icons_.size());
  EXPECT_TRUE(manager->valid_manifest());
  EXPECT_FALSE(
      (manager->icon_url(InstallableManager::IconUsage::kPrimary).is_empty()));
  EXPECT_NE(nullptr, (manager->icon(InstallableManager::IconUsage::kPrimary)));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
  EXPECT_EQ(NO_ERROR_DETECTED,
            (manager->icon_error(InstallableManager::IconUsage::kPrimary)));
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  EXPECT_TRUE(!manager->task_queue_.paused_tasks_.empty());

  {
    // Fetching just the manifest and icons should not hang while the other call
    // is waiting for a worker.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> nested_tester(
        new CallbackTester(run_loop.QuitClosure()));
    manager->GetData(
        GetPrimaryIconParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(nested_tester.get())));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(nested_tester->manifest()));
    EXPECT_FALSE(nested_tester->manifest_url().is_empty());
    EXPECT_FALSE(nested_tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, nested_tester->primary_icon());
    EXPECT_TRUE(nested_tester->valid_manifest());
    EXPECT_TRUE(nested_tester->worker_check_passed());
    EXPECT_TRUE(nested_tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, nested_tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, nested_tester->errors());
  }

  // Load the service worker.
  if (IsServiceWorkerOfflineSupported()) {
    EXPECT_TRUE(content::ExecJs(
        web_contents, "navigator.serviceWorker.register('service_worker.js');",
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  } else {
    EXPECT_TRUE(content::ExecJs(web_contents,
                                "navigator.serviceWorker.register("
                                "'service_worker_empty_fetch_handler.js');",
                                content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  }
  tester_run_loop.Run();

  // We should have passed now.
  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_TRUE(tester->valid_manifest());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  CheckServiceWorkerForTester(tester.get());

  // Verify internal state.
  EXPECT_FALSE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_FALSE(manager->manifest_url().is_empty());
  EXPECT_FALSE(manager->valid_manifest());
  EXPECT_EQ(1u, manager->icons_.size());
  EXPECT_FALSE(
      (manager->icon_url(InstallableManager::IconUsage::kPrimary).is_empty()));
  EXPECT_NE(nullptr, (manager->icon(InstallableManager::IconUsage::kPrimary)));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED,
            (manager->icon_error(InstallableManager::IconUsage::kPrimary)));
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  EXPECT_FALSE(!manager->task_queue_.paused_tasks_.empty());
  CheckServiceWorkerForInstallableManager(manager.get());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckLazyServiceWorkerNoFetchHandlerFails) {
  base::RunLoop tester_run_loop, sw_run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(tester_run_loop.QuitClosure()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto manager = std::make_unique<LazyWorkerInstallableManager>(
      web_contents, sw_run_loop.QuitClosure());

  // Load a URL with no service worker.
  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // Kick off fetching the data. This should block on waiting for a worker.
  manager->GetData(GetWebAppParams(),
                   base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                                  base::Unretained(tester.get())));
  sw_run_loop.Run();

  // We should now be waiting for the service worker.
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  EXPECT_TRUE(!manager->task_queue_.paused_tasks_.empty());

  // Load the service worker with no fetch handler.
  EXPECT_TRUE(content::ExecJs(web_contents,
                              "navigator.serviceWorker.register('"
                              "service_worker_no_fetch_handler.js');",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  tester_run_loop.Run();

  // We should fail the check.
  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_TRUE(tester->valid_manifest());
  EXPECT_FALSE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{NOT_OFFLINE_CAPABLE},
            tester->errors());
}

IN_PROC_BROWSER_TEST_P(InstallableManagerOfflineCapabilityBrowserTest,
                       CheckServiceWorkerErrorIsNotCached) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::RunLoop sw_run_loop;
  auto manager = std::make_unique<LazyWorkerInstallableManager>(
      web_contents, sw_run_loop.QuitClosure());

  // Load a URL with no service worker.
  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  {
    base::RunLoop tester_run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(tester_run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.wait_for_worker = false;
    manager->GetData(
        params, base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                               base::Unretained(tester.get())));
    tester_run_loop.Run();

    // We should have returned with an error.
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_TRUE(tester->valid_manifest());
    EXPECT_FALSE(tester->worker_check_passed());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_MATCHING_SERVICE_WORKER},
              tester->errors());
  }

  {
    base::RunLoop tester_run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(tester_run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    manager->GetData(
        params, base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                               base::Unretained(tester.get())));
    sw_run_loop.Run();

    if (IsServiceWorkerOfflineSupported()) {
      EXPECT_TRUE(content::ExecJs(
          web_contents,
          "navigator.serviceWorker.register('service_worker.js');",
          content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    } else {
      EXPECT_TRUE(content::ExecJs(web_contents,
                                  "navigator.serviceWorker.register("
                                  "'service_worker_empty_fetch_handler.js');",
                                  content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
    }
    tester_run_loop.Run();

    // The callback result will depend on the state of offline support.
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_TRUE(tester->valid_manifest());
    CheckServiceWorkerForTester(tester.get());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckPageWithNoServiceWorkerFetchHandler) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      browser(), tester.get(), GetWebAppParams(),
      "/banners/no_sw_fetch_handler_test_page.html");

  RunInstallableManager(browser(), tester.get(), GetWebAppParams());
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_FALSE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{NOT_OFFLINE_CAPABLE},
            tester->errors());
}

IN_PROC_BROWSER_TEST_P(InstallableManagerOfflineCapabilityBrowserTest,
                       CheckPageWithNestedServiceWorkerCanBeInstalled) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(browser(), tester.get(), GetWebAppParams(),
                                   GetPath("/banners/nested_sw_test_page"));

  auto params = GetWebAppParams();
  // Make sure valid_manifest check is run.
  params.is_debug_mode = true;
  RunInstallableManager(browser(), tester.get(), params);
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_TRUE(tester->valid_manifest());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  CheckServiceWorkerForTester(tester.get());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckDataUrlIcon) {
  // Verify that InstallableManager can handle data URL icons.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(browser(), tester.get(), GetWebAppParams(),
                                   GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/manifest_data_url_icon.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_EQ(144, tester->primary_icon()->width());
  EXPECT_TRUE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestCorruptedIcon) {
  // Verify that the returned InstallableData::primary_icon is null if the web
  // manifest points to a corrupt primary icon.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(browser(), tester.get(),
                                   GetPrimaryIconParams(),
                                   GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/manifest_bad_icon.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{NO_ICON_AVAILABLE},
            tester->errors());
}

IN_PROC_BROWSER_TEST_P(InstallableManagerOfflineCapabilityBrowserTest,
                       CheckChangeInIconDimensions) {
  // Verify that a follow-up request for a primary icon with a different size
  // works.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(browser(), tester.get(), GetWebAppParams(),
                                     GetPath("/banners/manifest_test_page"));
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->valid_manifest());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    CheckServiceWorkerForTester(tester.get());
  }

  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    auto params = GetWebAppParams();
    // Make sure valid_manifest check is run.
    params.is_debug_mode = true;
    RunInstallableManager(browser(), tester.get(), params);

    run_loop.Run();

    // The smaller primary icon requirements should allow this to pass.
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->valid_manifest());
    EXPECT_TRUE(tester->splash_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->splash_icon());
    CheckServiceWorkerForTester(tester.get());
  }
}

// The case that a service worker doesn't return an offline response for the
// start_url, but does for the manifest scope.
// - manifest's scope: /banners/
// - manifest's start_url: /banners/manifest_test_page.html?ignore
// - service worker's scope: /banners/
IN_PROC_BROWSER_TEST_P(InstallableManagerOfflineCapabilityBrowserTest,
                       CheckNotOfflineCapableStartUrl) {
  // This test wants to check the service worker that doesn't support offline
  // pages, so ignore the cases when `is_service_worker_offline_supported_` is
  // true.
  if (IsServiceWorkerOfflineSupported())
    return;

  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      browser(), tester.get(), GetWebAppParams(),
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_not_offline_capable_url.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_TRUE(tester->valid_manifest());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  CheckServiceWorkerForTester(tester.get());

  InstallableManager* manager = GetManager(browser());

  EXPECT_FALSE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_FALSE(manager->manifest_url().is_empty());
  EXPECT_FALSE(manager->valid_manifest());
  EXPECT_EQ(1u, manager->icons_.size());
  EXPECT_FALSE(
      (manager->icon_url(InstallableManager::IconUsage::kPrimary).is_empty()));
  EXPECT_NE(nullptr, (manager->icon(InstallableManager::IconUsage::kPrimary)));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED,
            (manager->icon_error(InstallableManager::IconUsage::kPrimary)));
  EXPECT_TRUE(!manager->task_queue_.HasCurrent());
  CheckServiceWorkerForInstallableManager(manager);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckNestedCallsToGetData) {
  // Verify that we can call GetData while in a callback from GetData.
  base::RunLoop run_loop;
  InstallableParams params = GetWebAppParams();
  std::unique_ptr<NestedCallbackTester> tester(new NestedCallbackTester(
      GetManager(browser()), params, run_loop.QuitClosure()));

  tester->Run();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManifestUrlChangeFlushesState) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto manager = std::make_unique<ResetDataInstallableManager>(web_contents);

  // Start on a page with no manifest.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html")));

  {
    // Fetch the data. This should return an empty manifest.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    EXPECT_TRUE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(NO_MANIFEST, manager->manifest_error());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_MANIFEST},
              tester->errors());
  }

  {
    // Injecting a manifest URL but not navigating should flush the state.
    base::RunLoop run_loop;
    manager->SetQuitClosure(run_loop.QuitClosure());
    EXPECT_TRUE(content::ExecJs(web_contents, "addManifestLinkTag()"));
    run_loop.Run();

    EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  }

  {
    // Fetch the data again. This should succeed.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
    EXPECT_EQ(u"Manifest test app", tester->manifest().name);
    EXPECT_EQ(std::u16string(),
              tester->manifest().short_name.value_or(std::u16string()));
  }

  {
    // Flush the state again by changing the manifest URL.
    base::RunLoop run_loop;
    manager->SetQuitClosure(run_loop.QuitClosure());

    GURL manifest_url = embedded_test_server()->GetURL(
        "/banners/manifest_short_name_only.json");
    EXPECT_TRUE(content::ExecJs(
        web_contents, "changeManifestUrl('" + manifest_url.spec() + "');"));
    run_loop.Run();

    EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  }

  {
    // Fetch again. This should return the data from the new manifest.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::u16string(),
              tester->manifest().name.value_or(std::u16string()));
    EXPECT_EQ(u"Manifest", tester->manifest().short_name);
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, DebugModeWithNoManifest) {
  // Ensure that a page with no manifest stops with NO_MANIFEST in debug mode.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetWebAppParams();
  params.is_debug_mode = true;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html")));
  RunInstallableManager(browser(), tester.get(), params);
  run_loop.Run();

  EXPECT_EQ(std::vector<InstallableStatusCode>({NO_MANIFEST}),
            tester->errors());
}

IN_PROC_BROWSER_TEST_P(InstallableManagerOfflineCapabilityBrowserTest,
                       DebugModeAccumulatesErrorsWithManifest) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetWebAppParams();
  params.is_debug_mode = true;
  NavigateAndRunInstallableManager(browser(), tester.get(), params,
                                   GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/play_app_manifest.json"));
  run_loop.Run();

  if (IsCheckOfflineCapableFeatureEnabled()) {
    EXPECT_EQ(
        std::vector<InstallableStatusCode>(
            {START_URL_NOT_VALID, MANIFEST_MISSING_NAME_OR_SHORT_NAME,
             MANIFEST_DISPLAY_NOT_SUPPORTED, MANIFEST_MISSING_SUITABLE_ICON,
             NO_URL_FOR_SERVICE_WORKER, NO_ACCEPTABLE_ICON}),
        tester->errors());
  } else {
    EXPECT_EQ(std::vector<InstallableStatusCode>(
                  {START_URL_NOT_VALID, MANIFEST_MISSING_NAME_OR_SHORT_NAME,
                   MANIFEST_DISPLAY_NOT_SUPPORTED,
                   MANIFEST_MISSING_SUITABLE_ICON, NO_ACCEPTABLE_ICON}),
              tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       DebugModeBadFallbackMaskable) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetPrimaryIconPreferMaskableParams();
  params.is_debug_mode = true;

  NavigateAndRunInstallableManager(
      browser(), tester.get(), params,
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_one_bad_maskable.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());

  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_FALSE(tester->has_maskable_splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{NO_ACCEPTABLE_ICON},
            tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       GetAllInstallabilityErrorsNoErrors) {
  EXPECT_EQ(std::vector<content::InstallabilityError>{},
            NavigateAndGetAllInstallabilityErrors(
                browser(), "/banners/manifest_test_page.html"));

  // Should pass a second time with no issues.
  EXPECT_EQ(std::vector<content::InstallabilityError>{},
            GetAllInstallabilityErrors(browser()));
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       GetAllInstallabilityErrorsWithNoManifest) {
  EXPECT_EQ(std::vector<content::InstallabilityError>{GetInstallabilityError(
                NO_MANIFEST)},
            NavigateAndGetAllInstallabilityErrors(
                browser(), "/banners/no_manifest_test_page.html"));

  // Should pass a second time with no issues.
  EXPECT_EQ(std::vector<content::InstallabilityError>{GetInstallabilityError(
                NO_MANIFEST)},
            GetAllInstallabilityErrors(browser()));
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       GetAllInstallabilityErrorsWithPlayAppManifest) {
  auto errors = std::vector<content::InstallabilityError>(
      {GetInstallabilityError(START_URL_NOT_VALID),
       GetInstallabilityError(MANIFEST_MISSING_NAME_OR_SHORT_NAME),
       GetInstallabilityError(MANIFEST_DISPLAY_NOT_SUPPORTED),
       GetInstallabilityError(MANIFEST_MISSING_SUITABLE_ICON)});
  errors.push_back(GetInstallabilityError(NO_ACCEPTABLE_ICON));
  EXPECT_EQ(errors, NavigateAndGetAllInstallabilityErrors(
                        browser(), GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/play_app_manifest.json")));
}

IN_PROC_BROWSER_TEST_F(InstallableManagerAllowlistOriginBrowserTest,
                       SecureOriginCheckRespectsUnsafeFlag) {
  // The allowlisted origin should be regarded as secure.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kInsecureOrigin)));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(InstallableManager::IsContentSecure(contents));

  // While a non-allowlisted origin should not.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(kOtherInsecureOrigin)));
  EXPECT_FALSE(InstallableManager::IsContentSecure(contents));
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, NarrowServiceWorker) {
  NavigateAndMaybeWaitForWorker(browser(), "/banners/scope_c/scope_c.html");
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetWebAppParams();
  params.wait_for_worker = false;

  RunInstallableManager(browser(), tester.get(), params);
  run_loop.Run();

  EXPECT_EQ(std::vector<InstallableStatusCode>({NO_MATCHING_SERVICE_WORKER}),
            tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckSplashIcon) {
  // Checks that InstallableManager chooses the correct splash icon.

  // Test page has a manifest with only one icon, primary icon and splash icon
  // should be the same one.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(browser(), tester.get(),
                                     GetPrimaryIconAndSplashIconParams(),
                                     GetURLOfPageWithServiceWorkerAndManifest(
                                         "/banners/manifest_one_icon.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());

    EXPECT_EQ(tester->primary_icon_url(), tester->splash_icon_url());
  }

  // Test page has a manifest with only one maskable icon. This should fail to
  // get a splash icon but not record an error.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        browser(), tester.get(), GetPreferMaskablePrimaryAndSplashIconParams(),
        GetURLOfPageWithServiceWorkerAndManifest(
            "/banners/manifest_one_maskable.json"));

    run_loop.Run();

    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->has_maskable_primary_icon());
    EXPECT_FALSE(tester->valid_manifest());
    EXPECT_TRUE(tester->worker_check_passed());
    EXPECT_FALSE(tester->splash_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->splash_icon());
    EXPECT_TRUE(tester->has_maskable_splash_icon());
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckScreenshots) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  NavigateAndRunInstallableManager(
      browser(), tester.get(), params,
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_with_screenshots.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_EQ(1u, tester->screenshots().size());
  // Corresponding form_factor should filter out the screenshot with mismatched
  // form_factor.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_LT(tester->screenshots()[0].image.width(),
            tester->screenshots()[0].image.height());
#else
  EXPECT_GT(tester->screenshots()[0].image.width(),
            tester->screenshots()[0].image.height());
#endif
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckScreenshotsPlatform) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  size_t num_of_screenshots = 0;
#if BUILDFLAG(IS_ANDROID)
  NavigateAndRunInstallableManager(
      browser(), tester.get(), params,
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_with_narrow_screenshots.json"));
  // Screenshots with unspecified form_factor is not filtered out.
  num_of_screenshots = 2;
  EXPECT_EQ(2u, tester->screenshots().size());
#else
  NavigateAndRunInstallableManager(
      browser(), tester.get(), params,
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_with_wide_screenshots.json"));
  // Screenshots with unspecified form_factor is filtered out.
  num_of_screenshots = 1;
#endif
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_EQ(num_of_screenshots, tester->screenshots().size());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckScreenshotsNumber) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  NavigateAndRunInstallableManager(
      browser(), tester.get(), params,
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_with_too_many_screenshots.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_EQ(8u, tester->screenshots().size());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckLargeScreenshotsFilteredOut) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params = GetManifestParams();
  params.fetch_screenshots = true;

  NavigateAndRunInstallableManager(
      browser(), tester.get(), params,
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_large_screenshot.json"));

  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_EQ(1u, tester->screenshots().size());
  EXPECT_EQ(551, tester->screenshots()[0].image.width());
  EXPECT_EQ(541, tester->screenshots()[0].image.height());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManifestLinkChangeReportsError) {
  InstallableManager* manager = GetManager(browser());

  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(browser(), tester.get(), GetManifestParams(),
                                   "/banners/manifest_test_page.html");
  // Simulate a manifest URL update by just calling the observer function.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  static_cast<content::WebContentsObserver*>(manager)->DidUpdateWebManifestURL(
      web_contents->GetPrimaryMainFrame(), GURL());
  run_loop.Run();

  ASSERT_EQ(tester->errors().size(), 1u);
  EXPECT_EQ(tester->errors()[0], MANIFEST_URL_CHANGED);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestOnly_DisplayOverride) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      browser(), tester.get(), GetManifestParams(),
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_display_override.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  ASSERT_EQ(2u, tester->manifest().display_override.size());
  EXPECT_EQ(blink::mojom::DisplayMode::kMinimalUi,
            tester->manifest().display_override[0]);
  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone,
            tester->manifest().display_override[1]);

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->has_maskable_primary_icon());
  EXPECT_FALSE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_FALSE(tester->has_maskable_splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManifestDisplayOverrideReportsError_DisplayOverride) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      browser(), tester.get(), GetWebAppParams(),
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_display_override_contains_browser.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  ASSERT_EQ(3u, tester->manifest().display_override.size());
  EXPECT_EQ(blink::mojom::DisplayMode::kBrowser,
            tester->manifest().display_override[0]);
  EXPECT_EQ(blink::mojom::DisplayMode::kMinimalUi,
            tester->manifest().display_override[1]);
  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone,
            tester->manifest().display_override[2]);
  EXPECT_EQ(
      std::vector<InstallableStatusCode>{
          MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED},
      tester->errors());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       FallbackToDisplayBrowser_DisplayOverride) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      browser(), tester.get(), GetWebAppParams(),
      GetURLOfPageWithServiceWorkerAndManifest(
          "/banners/manifest_display_override_display_is_browser.json"));
  run_loop.Run();

  EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
  EXPECT_FALSE(tester->manifest_url().is_empty());
  ASSERT_EQ(1u, tester->manifest().display_override.size());
  EXPECT_EQ(blink::mojom::DisplayMode::kStandalone,
            tester->manifest().display_override[0]);

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_EQ(144, tester->primary_icon()->width());
  EXPECT_TRUE(tester->valid_manifest());
  EXPECT_TRUE(tester->worker_check_passed());
  EXPECT_TRUE(tester->splash_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->splash_icon());
  EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
}

class InstallableManagerInPrerenderingBrowserTest
    : public InstallableManagerBrowserTest {
 public:
  InstallableManagerInPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &InstallableManagerInPrerenderingBrowserTest::web_contents,
            base::Unretained(this))) {}
  ~InstallableManagerInPrerenderingBrowserTest() override = default;

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(InstallableManagerInPrerenderingBrowserTest,
                       InstallableManagerInPrerendering) {
  auto manager = std::make_unique<ResetDataInstallableManager>(web_contents());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  manager->ClearOnResetData();

  // Loads a page in the prerendering.
  const std::string path = "/banners/manifest_test_page.html";
  auto prerender_url = embedded_test_server()->GetURL(path);
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);

  // The prerendering should not affect the current data.
  EXPECT_FALSE(manager->GetOnResetData());

  {
    // Fetches the data.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
  }
  // It should have no data since manifest_test_page.html is loaded in the
  // prerendering.
  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(NO_MANIFEST, manager->manifest_error());

  {
    // If the page is activated from the prerendering and the data should be
    // reset.
    base::RunLoop run_loop;
    manager->SetQuitClosure(run_loop.QuitClosure());
    NavigateAndMaybeWaitForWorker(browser(), path);
    run_loop.Run();
  }

  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());

  {
    // Fetch the data again. This should succeed.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
    EXPECT_EQ(u"Manifest test app", tester->manifest().name);
    EXPECT_EQ(std::u16string(),
              tester->manifest().short_name.value_or(std::u16string()));
  }
}

class MockInstallableManager : public InstallableManager {
 public:
  explicit MockInstallableManager(content::WebContents* web_contents)
      : InstallableManager(web_contents) {}
  ~MockInstallableManager() override = default;

  MOCK_METHOD(void, OnResetData, (), (override));
  MOCK_METHOD(void,
              DidUpdateWebManifestURL,
              (content::RenderFrameHost * rfh, const GURL& manifest_url),
              (override));
};

MATCHER_P(IsManifestURL, file_name, std::string()) {
  return arg.ExtractFileName() == file_name;
}

MATCHER_P(IsPrerenderedRFH, render_frame_host, std::string()) {
  return arg->GetGlobalId() == render_frame_host->GetGlobalId();
}

// Tests that NotifyManifestUrlChanged is called on the page that has manifest
// after the activation from the prerendering.
IN_PROC_BROWSER_TEST_F(InstallableManagerInPrerenderingBrowserTest,
                       NotifyManifestUrlChangedInActivation) {
  auto manager = std::make_unique<MockInstallableManager>(web_contents());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  // OnResetData() is called when a navigation is finished.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Loads a page in the prerendering.
  auto prerender_url =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  // OnResetData() should not be called on the prerendering.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(0);
  int host_id = prerender_helper()->AddPrerender(prerender_url);

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  content::RenderFrameHost* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  {
    // Fetches the data.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
  }
  // It should have no data since manifest_test_page.html is loaded in the
  // prerendering.
  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(NO_MANIFEST, manager->manifest_error());

  {
    // If the page is activated from the prerendering and the data should be
    // reset and notify the updated manifest url.
    EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
    EXPECT_CALL(*manager.get(),
                DidUpdateWebManifestURL(IsPrerenderedRFH(render_frame_host),
                                        IsManifestURL("manifest.json")));
    prerender_helper()->NavigatePrimaryPage(prerender_url);
  }

  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());

  {
    // Fetch the data again. This should succeed.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    EXPECT_FALSE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(std::vector<InstallableStatusCode>{}, tester->errors());
    EXPECT_EQ(u"Manifest test app", tester->manifest().name);
    EXPECT_EQ(std::u16string(),
              tester->manifest().short_name.value_or(std::u16string()));
  }
}

// Tests that NotifyManifestUrlChanged is not called without manifest after
// the activation from the prerendering.
IN_PROC_BROWSER_TEST_F(InstallableManagerInPrerenderingBrowserTest,
                       NotNotifyManifestUrlChangedInActivation) {
  auto manager = std::make_unique<MockInstallableManager>(web_contents());
  GURL url = embedded_test_server()->GetURL("/empty.html");
  // OnResetData() is called when a navigation is finished.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Loads a page in the prerendering.
  auto prerender_url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  // OnResetData() should not be called on the prerendering.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(0);
  int host_id = prerender_helper()->AddPrerender(prerender_url);

  content::test::PrerenderHostObserver host_observer(*web_contents(), host_id);
  {
    // Fetches the data.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
  }
  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(NO_MANIFEST, manager->manifest_error());

  // OnResetData() is called when a navigation is finished.
  EXPECT_CALL(*manager.get(), OnResetData()).Times(1);
  // OnResetData() should not be called when a page doesn't have a manifest.
  EXPECT_CALL(*manager.get(), DidUpdateWebManifestURL(testing::_, testing::_))
      .Times(0);
  prerender_helper()->NavigatePrimaryPage(prerender_url);

  EXPECT_TRUE(host_observer.was_activated());
  EXPECT_TRUE(blink::IsEmptyManifest(manager->manifest()));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());

  {
    // Fetch the data again. This should return the same empty result as
    // earlier.
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    manager->GetData(
        GetWebAppParams(),
        base::BindOnce(&CallbackTester::OnDidFinishInstallableCheck,
                       base::Unretained(tester.get())));
    run_loop.Run();
    EXPECT_TRUE(blink::IsEmptyManifest(tester->manifest()));
    EXPECT_EQ(NO_MANIFEST, manager->manifest_error());
    EXPECT_EQ(std::vector<InstallableStatusCode>{NO_MANIFEST},
              tester->errors());
  }
}

}  // namespace webapps
