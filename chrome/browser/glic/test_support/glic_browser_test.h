// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_TEST_H_

#include <map>
#include <memory>
#include <sstream>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace glic {

#if BUILDFLAG(IS_ANDROID)
#define SKIP_TEST_FOR_NON_DESKTOP_ANDROID()            \
  if (!base::android::device_info::is_desktop()) {     \
    GTEST_SKIP() << "Skipping on non-desktop Android"; \
  }
#else
#define SKIP_TEST_FOR_NON_DESKTOP_ANDROID()
#endif

#if BUILDFLAG(IS_ANDROID)
#define SKIP_NEEDS_ANDROID_IMPL(message) \
  if (true) {                            \
    GTEST_SKIP() << message;             \
  }
#else
#define SKIP_NEEDS_ANDROID_IMPL(message)
#endif

template <typename Trigger>
[[nodiscard]] bool RunUntil(Trigger&& trigger, std::string_view message) {
  if (base::test::RunUntil(std::forward<Trigger>(trigger))) {
    return true;
  }
  LOG(ERROR) << message;
  return false;
}

class GlicInstanceImpl;

template <typename T>
class GlicBrowserTestMixin : public T {
 public:
  template <typename... Args>
  explicit GlicBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicMultiInstance);
  }
  ~GlicBrowserTestMixin() override = default;

  // PlatformBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    T::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_DESKTOP_ANDROID)
    // This is needed to force is_desktop() to return true for desktop Android
    // builds.
    command_line->AppendSwitch(switches::kForceDesktopAndroid);
#endif
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    browser_activator_ = std::make_unique<BrowserActivator>();

    CHECK(glic_test_environment_.SetupEmbeddedTestServers(
        T::embedded_test_server(), &T::embedded_https_test_server()));
    LOG(INFO) << "GlicBrowserTest: done setting up";
  }

  void TearDownOnMainThread() override {
    browser_activator_.reset();
    T::TearDownOnMainThread();
  }

  // Toggles the Glic UI.
  // If `prevent_close` is true, the Glic window will be set to prevent
  // closing on deactivation (if applicable).
  void ToggleGlicForActiveTab(bool prevent_close = false) {
    auto* service = GlicKeyedService::Get(T::GetProfile());
    service->ToggleUI(
        T::GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface(),
        prevent_close, mojom::InvocationSource::kTopChromeButton);
  }

  // Opens the Glic UI on the active tab and returns it.
  [[nodiscard]] GlicInstanceImpl* OpenGlicForActiveTab() {
    ToggleGlicForActiveTab(/*prevent_close=*/true);
    GlicInstanceImpl* instance =
        WaitForGlicOpen(T::GetTabListInterface()->GetActiveTab());
    if (!instance) {
      LOG(ERROR) << "Failed to open Glic for active tab";
    }
    return instance;
  }

  // Waits for the Glic UI to be open and visible. Defaults to the only glic
  // instance, but can be specified. Returns the opened instance or nullptr if
  // it fails to open.
  [[nodiscard]] GlicInstanceImpl* WaitForGlicOpen(
      GlicInstance* instance = nullptr) {
    std::optional<InstanceId> id =
        instance ? std::make_optional(instance->id()) : std::nullopt;
    bool success = RunUntil(
        [this, id]() {
          GlicInstance* target =
              id ? GetInstanceById(*id) : GetOnlyGlicInstance();
          return target && target->IsShowing();
        },
        "Failed to wait for Glic to open");
    if (!success) {
      return nullptr;
    }
    return id ? GetInstanceById(*id) : GetOnlyGlicInstance();
  }

  // Waits for the Glic UI to be open and visible for a specific tab. Returns
  // the opened instance or nullptr if it fails to open.
  [[nodiscard]] GlicInstanceImpl* WaitForGlicOpen(tabs::TabInterface* tab) {
    bool success = RunUntil(
        [this, tab]() {
          GlicInstance* instance = GetInstanceForTab(tab);
          return instance && instance->IsShowing();
        },
        "Failed to wait for Glic to open for tab");
    if (!success) {
      return nullptr;
    }
    return GetInstanceForTab(tab);
  }

  // Waits for the Glic UI to be closed. Defaults to the only glic instance,
  // but can be specified.
  [[nodiscard]] bool WaitForGlicClose(GlicInstance* instance = nullptr) {
    std::optional<InstanceId> id =
        instance ? std::make_optional(instance->id()) : std::nullopt;
    return RunUntil(
        [this, id]() {
          GlicInstance* target =
              id ? GetInstanceById(*id) : GetOnlyGlicInstance();
          return !target || !target->IsShowing();
        },
        "Failed to close Glic UI");
  }

  [[nodiscard]] GlicInstanceImpl* WaitForGlicInstanceBoundToTab(
      tabs::TabInterface* tab) {
    bool success = RunUntil(
        [this, tab]() {
          GlicInstanceImpl* instance = GetInstanceForTab(tab);
          return instance;
        },
        "Failed to wait for Glic to be bound to tab");
    if (!success) {
      return nullptr;
    }
    return GetInstanceForTab(tab);
  }

  // Returns the only glic instance. CHECK fails if there is ever more than one.
  GlicInstanceImpl* GetOnlyGlicInstance() {
    CHECK(GlicEnabling::IsMultiInstanceEnabled());
    return static_cast<GlicInstanceImpl*>(
        ::glic::GetOnlyGlicInstance(T::GetProfile()));
  }

  // Returns the glic instance bound to the given tab. Returns nullptr if not
  // found.
  GlicInstanceImpl* GetInstanceForTab(tabs::TabInterface* tab) {
    CHECK(GlicEnabling::IsMultiInstanceEnabled());
    return static_cast<GlicInstanceImpl*>(
        ::glic::GetInstanceForTab(T::GetProfile(), tab));
  }

  // Returns the glic instance with the given id. Returns nullptr if not found.
  GlicInstanceImpl* GetInstanceById(InstanceId id) {
    CHECK(GlicEnabling::IsMultiInstanceEnabled());
    return static_cast<GlicInstanceImpl*>(
        ::glic::GetInstanceById(T::GetProfile(), id));
  }

  GlicInstanceCoordinatorImpl& coordinator() {
    CHECK(GlicEnabling::IsMultiInstanceEnabled());
    return static_cast<GlicInstanceCoordinatorImpl&>(
        GlicKeyedService::Get(T::GetProfile())->window_controller());
  }

  // Opens a new tab with the given URL.
  tabs::TabInterface* CreateAndActivateTab(const GURL& url) {
    tabs::TabInterface* new_tab = T::GetTabListInterface()->OpenTab(url, -1);
    T::GetTabListInterface()->ActivateTab(new_tab->GetHandle());
    return new_tab;
  }

  void SetGlicPagePath(const std::string& glic_page_path) {
    glic_test_environment_.SetGlicPagePath(glic_page_path);
  }

  // Adds a query param to the URL that will be used to load the mock glic.
  // Must be called before `SetUpOnMainThread()`. Both `key` and `value` (if
  // specified) will be URL-encoded for safety.
  void AddMockGlicQueryParam(const std::string_view& key,
                             const std::string_view& value = "") {
    glic_test_environment_.AddMockGlicQueryParam(key, value);
  }

  GURL GetGuestURL() { return glic_test_environment_.GetGuestURL(); }

  void SetGlicFreUrlOverride(const GURL& url) {
    glic_test_environment_.SetGlicFreUrlOverride(url);
  }

  BrowserActivator* browser_activator() { return browser_activator_.get(); }

 protected:
  GlicTestEnvironment& glic_test_environment() {
    return glic_test_environment_;
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<BrowserActivator> browser_activator_;
};

using GlicBrowserTest = GlicBrowserTestMixin<PlatformBrowserTest>;

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_TEST_H_
