// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_TEST_H_

#include <map>
#include <memory>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/function_ref.h"
#include "base/path_service.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_tab_added_waiter.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/test_result.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "ui/views/test/mock_activation_controller.h"
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

// Runs `get_value` until it returns `expected_value`. Returns a
// TestResult<> indicating success or failure.
// Note, `type_identity_t` ensures T's type is inferred from `expected_value`.
template <typename T>
[[nodiscard]] TestResult<> RunUntilEqual(
    base::FunctionRef<std::type_identity_t<T>()> get_value,
    const T& expected_value,
    std::string_view message = std::string_view()) {
  using ValueType = std::remove_reference_t<T>;
  if (get_value() == expected_value) {
    return base::ok();
  }
  std::vector<ValueType> ignored_values;
  if (base::test::RunUntil([get_value, expected_value, &ignored_values]() {
        ValueType value = get_value();
        if (value == expected_value) {
          return true;
        }
        if (ignored_values.empty() || ignored_values.back() != value) {
          ignored_values.push_back(value);
        }
        return false;
      })) {
    return base::ok();
  }
  std::stringstream ss;
  ss << message << " Expected: " << expected_value << ", saw values: {";
  for (const auto& value : ignored_values) {
    ss << value << ", ";
  }
  ss << "}";
  return base::unexpected(ss.str());
}

template <typename Trigger>
[[nodiscard]] bool RunUntil(Trigger&& trigger, std::string_view message) {
  if (base::test::RunUntil(std::forward<Trigger>(trigger))) {
    return true;
  }
  LOG(ERROR) << message;
  return false;
}

[[nodiscard]] inline TestResult<> WaitForSidePanelState(
    tabs::TabInterface* tab,
    GlicSidePanelCoordinator::State expected_state) {
  auto* side_panel_coordinator = GlicSidePanelCoordinator::GetForTab(tab);
  if (!side_panel_coordinator) {
    return base::unexpected("GlicSidePanelCoordinator not found for tab");
  }
  return RunUntilEqual([&]() { return side_panel_coordinator->state(); },
                       expected_state,
                       "Timeout waiting for side panel state to match");
}

class GlicInstanceImpl;

template <typename T>
class GlicBrowserTestMixin : public T {
 public:
  template <typename... Args>
  explicit GlicBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicMultiInstance, {}},
#if BUILDFLAG(IS_ANDROID)
        {chrome::android::kBrowserWindowInterfaceMobile, {}},
        {chrome::android::kTabBottomSheet, {}},
#endif
    // TODO(crbug.com/516793173): Remove this compile-time check once C++
    // browser tests automatically inherit --force-desktop-android just like
    // Java.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
        {chrome::android::kEnableAndroidSidePanel, {}},
        {chrome::android::kEnableAndroidSidePanelLogs, {}},
        {features::kGlicAndroidSidePanel, {}},
#endif
    };

    glic_test_environment_.SetGlicPagePath(
        "/glic/browser_tests/minimal_client.html");
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
    // Globally block all In-Product Help (IPH) triggers in all Glic browser
    // tests to avoid flakiness caused by unexpected IPH popups (e.g., adaptive
    // top toolbar customization cues on Android), which can make glic hide.
    scoped_iph_feature_list_.InitWithNoFeaturesAllowed();
  }
  ~GlicBrowserTestMixin() override = default;

  // PlatformBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    T::SetUpCommandLine(command_line);
    // TODO(crbug.com/516793173): Remove this switch once C++ browser tests
    // automatically inherit --force-desktop-android just like Java.
#if BUILDFLAG(IS_DESKTOP_ANDROID)
    // This is needed to force is_desktop() to return true for desktop Android
    // builds.
    command_line->AppendSwitch(switches::kForceDesktopAndroid);
#endif
#if BUILDFLAG(IS_ANDROID)
    // Disable the first-run experience (FRE) so that when we launch a new
    // ChromeTabbedActivity in tests, it shows the browser window instead of the
    // FRE onboarding screens.
    command_line->AppendSwitch("disable-fre");
#endif
  }

  void SetUp() override {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_S) {
      GTEST_SKIP() << "Glic requires Android S+ to run";
    }
#endif
    T::SetUp();
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
    activation_controller_ =
        std::make_unique<views::test::MockActivationController>();
#endif
#if defined(TOOLKIT_VIEWS)
    SidePanelCoordinator::From(GetBrowser())->DisableAnimationsForTesting();
#endif

    CHECK(glic_test_environment_.SetupEmbeddedTestServers(
        T::embedded_test_server(), &T::embedded_https_test_server()));
    T::GetTabListInterface()
        ->GetActiveTab()
        ->GetBrowserWindowInterface()
        ->GetWindow()
        ->Activate();
    LOG(INFO) << "GlicBrowserTest: done setting up";
  }

  void TearDownOnMainThread() override {
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
    activation_controller_.reset();
#endif
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
  [[nodiscard]] TestResult<GlicInstanceImpl*> OpenGlicForActiveTab() {
    ToggleGlicForActiveTab(/*prevent_close=*/true);
    return WaitForGlicOpen(T::GetTabListInterface()->GetActiveTab());
  }

  void RegisterConversation(GlicInstance* instance,
                            const std::string& conversation_id) {
    CHECK(instance);
    auto info = mojom::ConversationInfo::New();
    info->conversation_id = conversation_id;
    static_cast<GlicInstanceImpl*>(instance)->RegisterConversation(
        std::move(info), base::DoNothing());
  }

  // Registers a conversation and submits input to prevent the instance from
  // being deleted when closed.
  void PreventDeletionOnClose(
      GlicInstanceImpl* instance = nullptr,
      const std::string& conversation_id = "test_conversation") {
    if (!instance) {
      instance = GetOnlyGlicInstance();
    }
    CHECK(instance);
    if (!instance->conversation_id().has_value()) {
      RegisterConversation(instance, conversation_id);
    }
    instance->OnUserInputSubmitted(mojom::WebClientMode::kText);
  }

  void CloseAllEmbeddersAndPreventDeletion(
      GlicInstanceImpl* instance = nullptr) {
    if (!instance) {
      instance = GetOnlyGlicInstance();
    }
    CHECK(instance);
    PreventDeletionOnClose(instance);
    instance->CloseAllEmbedders();
  }

  // Opens the Glic UI on the active tab and detaches it.
  [[nodiscard]] TestResult<GlicInstanceImpl*> OpenGlicForActiveTabAndDetach() {
    ASSIGN_OR_RETURN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
    if (instance->IsDetached()) {
      return instance;
    }
    instance->Detach(*T::GetTabListInterface()->GetActiveTab());
    bool success = RunUntil([instance]() { return instance->IsDetached(); },
                            "Failed to wait for Glic to detach");
    if (!success) {
      return base::unexpected("Failed to wait for Glic to detach");
    }
    return instance;
  }

  // Waits for the Glic UI to be open and visible. Defaults to the only glic
  // instance, but can be specified. Returns the opened instance or nullptr if
  // it fails to open.
  [[nodiscard]] TestResult<GlicInstanceImpl*> WaitForGlicOpen(
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
      return base::unexpected("Failed to wait for Glic to open");
    }
    auto* result = id ? GetInstanceById(*id) : GetOnlyGlicInstance();
    if (!result) {
      return base::unexpected("Glic instance not found after opening");
    }
    return result;
  }

  // Waits for the Glic UI to be open and visible for a specific tab. Returns
  // the opened instance or nullptr if it fails to open.
  [[nodiscard]] TestResult<GlicInstanceImpl*> WaitForGlicOpen(
      tabs::TabInterface* tab) {
    bool success = RunUntil(
        [this, tab]() {
          GlicInstance* instance = GetInstanceForTab(tab);
          return instance && instance->IsShowing();
        },
        "Failed to wait for Glic to open for tab");
    if (!success) {
      return base::unexpected("Failed to wait for Glic to open for tab");
    }
    auto* instance = GetInstanceForTab(tab);
    if (!instance) {
      return base::unexpected("Glic instance not found for tab after opening");
    }
    return instance;
  }

  // Waits for the Glic UI to be closed. Defaults to the only glic instance,
  // but can be specified.
  TestResult<> WaitForGlicClose(GlicInstance* instance = nullptr) {
    std::optional<InstanceId> id =
        instance ? std::make_optional(instance->id()) : std::nullopt;
    bool success = RunUntil(
        [this, id]() {
          GlicInstance* target =
              id ? GetInstanceById(*id) : GetOnlyGlicInstance();
          return !target || !target->IsShowing();
        },
        "Failed to close Glic UI");
    if (!success) {
      return base::unexpected("Failed to close Glic UI");
    }
    return base::ok();
  }

  // Closes Glic for a given tab and waits for it to close.
  TestResult<> CloseGlicForTabAndWait(tabs::TabInterface* tab) {
    GlicInstanceImpl* instance = GetInstanceForTab(tab);
    if (!instance) {
      return base::unexpected("No Glic instance found for tab to close");
    }
    instance->Close(tab);
    return WaitForGlicClose(instance);
  }

  [[nodiscard]] TestResult<GlicInstanceImpl*> WaitForGlicInstanceBoundToTab(
      tabs::TabInterface* tab) {
    bool success =
        RunUntil([this, tab]() { return GetInstanceForTab(tab) != nullptr; },
                 "Failed to wait for Glic to be bound to tab");
    if (!success) {
      return base::unexpected("Failed to wait for Glic to be bound to tab");
    }
    return GetInstanceForTab(tab);
  }

  // Returns the only glic instance. CHECK fails if there is ever more than one.
  GlicInstanceImpl* GetOnlyGlicInstance() {
    return static_cast<GlicInstanceImpl*>(
        ::glic::GetOnlyGlicInstance(T::GetProfile()));
  }

  // Returns the glic instance bound to the given tab. Returns nullptr if not
  // found.
  GlicInstanceImpl* GetInstanceForTab(tabs::TabInterface* tab) {
    return static_cast<GlicInstanceImpl*>(
        ::glic::GetInstanceForTab(T::GetProfile(), tab));
  }

  // Returns the glic instance with the given id. Returns nullptr if not found.
  GlicInstanceImpl* GetInstanceById(InstanceId id) {
    return static_cast<GlicInstanceImpl*>(
        ::glic::GetInstanceById(T::GetProfile(), id));
  }

  GlicInstanceCoordinatorImpl& coordinator() {
    return static_cast<GlicInstanceCoordinatorImpl&>(
        GlicKeyedService::Get(T::GetProfile())->instance_coordinator());
  }

  // Opens a new tab with the given URL and wait for load to complete.
  tabs::TabInterface* CreateAndActivateTab(const GURL& url) {
    tabs::TabInterface* new_tab = T::GetTabListInterface()->OpenTab(url, -1);
    T::GetTabListInterface()->ActivateTab(new_tab->GetHandle());
    CHECK(content::WaitForLoadStop(new_tab->GetContents()));
    return new_tab;
  }

  content::Visibility GetContentsVisibility(GlicInstanceImpl* instance) {
    EXPECT_TRUE(instance);
    content::WebContents* webui_contents = instance->host().webui_contents();
    return webui_contents ? webui_contents->GetVisibility()
                          : content::Visibility::HIDDEN;
  }

  std::string VisibilityAsString(content::Visibility visibility) {
    switch (visibility) {
      case content::Visibility::HIDDEN:
        return "HIDDEN";
      case content::Visibility::OCCLUDED:
        return "OCCLUDED";
      case content::Visibility::VISIBLE:
        return "VISIBLE";
    }
    return "UNKNOWN";
  }

  [[nodiscard]] TestResult<> WaitForWebUiContentsVisibility(
      GlicInstanceImpl* instance,
      content::Visibility visibility) {
    return RunUntilEqual(
        [&]() { return VisibilityAsString(GetContentsVisibility(instance)); },
        VisibilityAsString(visibility),
        "Timeout waiting for webui WebContents visibility to be " +
            VisibilityAsString(visibility));
  }

  void ActivateTab(tabs::TabInterface* tab) {
    CHECK(tab);
    tab->GetContents()->GetDelegate()->ActivateContents(tab->GetContents());
  }

  tabs::TabInterface* CreateUserInitiatedTab(const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
    auto* tab_list = T::GetTabListInterface();
    CHECK(tab_list) << "TabListInterface is null";
    auto* tab_model = static_cast<TabModel*>(tab_list);
    Profile* profile = T::GetProfile();
    CHECK(profile) << "Profile is null";

    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile));
    web_contents->GetController().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
    tabs::TabInterface* new_tab =
        tab_model->CreateTab(nullptr, std::move(web_contents), -1,
                             TabModel::TabLaunchType::FROM_CHROME_UI, false);
    tab_model->ActivateTab(new_tab->GetHandle());
    return new_tab;
#else
    return CreateAndActivateTab(url);
#endif
  }

  // Simulates a click on a link with the given modifiers.
  // On Android, this uses a tap with modifiers, and injects a viewport meta tag
  // to ensure coordinates are correct.
  void SimulateLinkClick(tabs::TabInterface* tab,
                         bool ctrl_key,
                         bool shift_key) {
    content::WebContents* contents = tab->GetContents();
    std::string link_id = "simulator-link";
    std::string script = base::StringPrintf(
        R"(
          (() => {
            const meta = document.createElement('meta');
            meta.name = 'viewport';
            meta.content = 'width=device-width,minimum-scale=1';
            document.head.appendChild(meta);

            const a = document.createElement('a');
            a.id = '%s';
            a.href = 'about:blank';
            a.innerText = 'Click me';
            a.style.position = 'fixed';
            a.style.left = '0';
            a.style.top = '0';
            a.style.width = '100vw';
            a.style.height = '100vh';
            a.style.zIndex = '9999';
            document.body.appendChild(a);
          })();
        )",
        link_id.c_str());

    content::RenderFrameSubmissionObserver frame_observer(contents);

    frame_observer.SetWaitForNextFrame();
    EXPECT_TRUE(content::ExecJs(contents, script));

    // Wait for the next frame to ensure the element is visible to the
    // compositor. Without this wait, the click might happen too early and not
    // trigger the navigation.
    frame_observer.WaitForNextFrameSubmission();

    int modifiers = 0;
    if (ctrl_key) {
#if BUILDFLAG(IS_MAC)
      modifiers |= blink::WebInputEvent::kMetaKey;
#else
      modifiers |= blink::WebInputEvent::kControlKey;
#endif
    }
    if (shift_key) {
      modifiers |= blink::WebInputEvent::kShiftKey;
    }

    gfx::Point point = gfx::ToFlooredPoint(
        content::GetCenterCoordinatesOfElementWithId(contents, link_id));

#if BUILDFLAG(IS_ANDROID)
    content::SimulateTapWithModifiersAt(contents, modifiers, point);
#else
    content::SimulateMouseClickAt(contents, modifiers,
                                  blink::WebMouseEvent::Button::kLeft, point);
#endif
  }

  TestResult<> WaitForActiveEmbedderToMatchTab(GlicInstanceImpl* instance,
                                               tabs::TabInterface* tab) {
    CHECK(tab);
    CHECK(instance);
    return RunUntilEqual(
        [&]() { return instance->GetActiveEmbedderTabForTesting(); }, tab,
        "Timeout waiting for active embedder to match tab");
  }

  TestResult<> WaitForEmbedderActivationOrPeek(GlicInstanceImpl* instance,
                                               tabs::TabInterface* tab) {
    CHECK(tab);
    CHECK(instance);

    auto* side_panel_coordinator = GlicSidePanelCoordinator::GetForTab(tab);
    bool supports_peek =
        side_panel_coordinator && side_panel_coordinator->SupportsPeek();

    if (supports_peek) {
      RETURN_IF_ERROR(RunUntilEqual(
          [&]() { return instance->GetEmbedderForTab(tab) != nullptr; }, true,
          "Timeout waiting for embedder to bind"));
      RETURN_IF_ERROR(
          WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kPeek));
      return WaitForWebUiContentsVisibility(instance,
                                            content::Visibility::HIDDEN);
    } else {
      RETURN_IF_ERROR(RunUntilEqual(
          [&]() { return instance->GetActiveEmbedderTabForTesting(); }, tab,
          "Timeout waiting for active embedder to match tab"));
      return WaitForWebUiContentsVisibility(instance,
                                            content::Visibility::VISIBLE);
    }
  }

  [[nodiscard]] TestResult<> WaitForWebUiState(mojom::WebUiState state) {
    auto state_to_string = [](mojom::WebUiState state) -> std::string {
      std::stringstream ss;
      ss << state;
      return ss.str();
    };
    return RunUntilEqual(
        [&]() -> std::string {
          GlicInstanceImpl* instance = GetOnlyGlicInstance();
          if (!instance) {
            return "no instance";
          }
          return state_to_string(instance->host().GetPrimaryWebUiState());
        },
        state_to_string(state));
  }

  GlicKeyedService* service() { return GlicKeyedService::Get(T::GetProfile()); }
  BrowserWindowInterface* GetBrowser() {
    return T::GetTabListInterface()
        ->GetActiveTab()
        ->GetBrowserWindowInterface();
  }

  // Returns a simple URL for testing that is guaranteed to load properly via
  // the embedded test server.
  GURL GetSimpleTestUrl() { return GetTestUrl("page.html"); }

  GURL GetTestUrl(const std::string& file_name) {
    return T::embedded_test_server()->GetURL("/test_data/" + file_name);
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

  [[nodiscard]] TestResult<void> WaitForGlicClient(GlicInstance* instance) {
    auto* instance_impl = GetInstanceImpl(instance);
    return RunUntilEqual(
        [&]() { return instance_impl->host().IsWebClientConnected(); }, true,
        "WaitForGlicClient: client client did not connect");
  }

  // Create an actor task for the instance.
  [[nodiscard]] TestResult<actor::TaskId> CreateActorTask(
      GlicInstance* instance = nullptr) {
    auto instance_impl = GetInstanceImpl(instance);
    base::test::TestFuture<
        base::expected<int32_t, glic::mojom::CreateTaskErrorReason>>
        create_task_future;
    RETURN_IF_ERROR(WaitForGlicClient(instance));
    instance_impl->GetActorTaskManager()
        ->GetClientSessionForTesting()
        ->CreateTask(actor::webui::mojom::TaskOptions::New(),
                     create_task_future.GetCallback());
    return create_task_future.Get()
        .transform(
            [](int32_t id) -> actor::TaskId { return actor::TaskId(id); })
        .transform_error([](const auto& e) -> std::string {
          std::stringstream ss;
          ss << "Failed to create actor task: " << e;
          return ss.str();
        });
  }

 protected:
  GlicTestEnvironment& glic_test_environment() {
    return glic_test_environment_;
  }

  GlicInstanceImpl* GetInstanceImpl(GlicInstance* instance = nullptr) {
    if (!instance) {
      instance = GetOnlyGlicInstance();
    }
    return static_cast<GlicInstanceImpl*>(instance);
  }

 private:
  // Hide functionality not available on Android to discourage use.
  // Callers can instead use, e.g. PlatformBrowserTest::browser(), or
  // redeclare PlatformBrowserTest::browser as public on the test fixture.
#if !BUILDFLAG(IS_ANDROID)
  // Alternative: CreateAndActivateTab(GURL("about:blank"))
  using T::AddBlankTabAndShow;
  // Alternative: GetBrowser()
  using T::browser;
  // Alternative:
  // chrome/browser/ui/browser_window/public/create_browser_window.h.
  using T::CreateBrowser;
  using T::CreateBrowserForApp;
  using T::CreateBrowserForPopup;
  using T::CreateIncognitoBrowser;
  using T::OpenURLOffTheRecord;
#endif
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  std::unique_ptr<views::test::MockActivationController> activation_controller_;
#endif
};

using GlicBrowserTest = GlicBrowserTestMixin<PlatformBrowserTest>;

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_TEST_H_
