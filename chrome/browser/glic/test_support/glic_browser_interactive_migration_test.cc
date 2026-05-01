// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_browser_interactive_migration_test.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

namespace glic {

GlicBrowserInteractiveMigrationTest::GlicBrowserInteractiveMigrationTest() {
  std::vector<base::test::FeatureRefAndParams> enabled_features = {
      {features::kGlicMultiInstance, {}},
#if BUILDFLAG(IS_ANDROID)
      {chrome::android::kBrowserWindowInterfaceMobile, {}},
      {chrome::android::kTabBottomSheet, {}},
#endif
  };
  glic_test_environment_.SetGlicPagePath(
      "/glic/browser_tests/minimal_client.html");
  scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
}

GlicBrowserInteractiveMigrationTest::~GlicBrowserInteractiveMigrationTest() =
    default;

void GlicBrowserInteractiveMigrationTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  InteractiveBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  command_line->AppendSwitch(switches::kForceDesktopAndroid);
#endif
}

void GlicBrowserInteractiveMigrationTest::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  activation_controller_ =
      std::make_unique<views::test::MockActivationController>();
#endif

  CHECK(glic_test_environment_.SetupEmbeddedTestServers(
      embedded_test_server(), &embedded_https_test_server()));
  TabListInterface::From(browser())
      ->GetActiveTab()
      ->GetBrowserWindowInterface()
      ->GetWindow()
      ->Activate();
  LOG(INFO) << "GlicBrowserInteractiveMigrationTest: done setting up";
}

void GlicBrowserInteractiveMigrationTest::TearDownOnMainThread() {
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  activation_controller_.reset();
#endif
  InteractiveBrowserTest::TearDownOnMainThread();
}

void GlicBrowserInteractiveMigrationTest::ToggleGlicForActiveTab(
    bool prevent_close) {
  auto* service = GlicKeyedService::Get(browser()->profile());
  service->ToggleUI(TabListInterface::From(browser())
                        ->GetActiveTab()
                        ->GetBrowserWindowInterface(),
                    prevent_close, mojom::InvocationSource::kTopChromeButton);
}

TestResult<GlicInstanceImpl*>
GlicBrowserInteractiveMigrationTest::OpenGlicForActiveTab() {
  ToggleGlicForActiveTab(/*prevent_close=*/true);
  return WaitForGlicOpen(TabListInterface::From(browser())->GetActiveTab());
}

void GlicBrowserInteractiveMigrationTest::PreventDeletionOnClose(
    GlicInstanceImpl* instance,
    const std::string& conversation_id) {
  if (!instance) {
    instance = GetOnlyGlicInstance();
  }
  CHECK(instance);
  if (!instance->conversation_id().has_value()) {
    auto info = mojom::ConversationInfo::New();
    info->conversation_id = conversation_id;
    instance->RegisterConversation(std::move(info), base::DoNothing());
  }
  instance->OnUserInputSubmitted(mojom::WebClientMode::kText);
}

void GlicBrowserInteractiveMigrationTest::CloseAllEmbeddersAndPreventDeletion(
    GlicInstanceImpl* instance) {
  if (!instance) {
    instance = GetOnlyGlicInstance();
  }
  CHECK(instance);
  PreventDeletionOnClose(instance);
  instance->CloseAllEmbedders();
}

TestResult<GlicInstanceImpl*>
GlicBrowserInteractiveMigrationTest::OpenGlicForActiveTabAndDetach() {
  ASSIGN_OR_RETURN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  if (instance->IsDetached()) {
    return instance;
  }
  instance->Detach(*TabListInterface::From(browser())->GetActiveTab());
  bool success = RunUntil([instance]() { return instance->IsDetached(); },
                          "Failed to wait for Glic to detach");
  if (!success) {
    return base::unexpected("Failed to wait for Glic to detach");
  }
  return instance;
}

TestResult<GlicInstanceImpl*>
GlicBrowserInteractiveMigrationTest::WaitForGlicOpen(GlicInstance* instance) {
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

TestResult<GlicInstanceImpl*>
GlicBrowserInteractiveMigrationTest::WaitForGlicOpen(tabs::TabInterface* tab) {
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

TestResult<> GlicBrowserInteractiveMigrationTest::WaitForGlicClose(
    GlicInstance* instance) {
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

TestResult<> GlicBrowserInteractiveMigrationTest::CloseGlicForTabAndWait(
    tabs::TabInterface* tab) {
  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  if (!instance) {
    return base::unexpected("No Glic instance found for tab to close");
  }
  instance->Close(tab);
  return WaitForGlicClose(instance);
}

TestResult<GlicInstanceImpl*>
GlicBrowserInteractiveMigrationTest::WaitForGlicInstanceBoundToTab(
    tabs::TabInterface* tab) {
  bool success =
      RunUntil([this, tab]() { return GetInstanceForTab(tab) != nullptr; },
               "Failed to wait for Glic to be bound to tab");
  if (!success) {
    return base::unexpected("Failed to wait for Glic to be bound to tab");
  }
  return GetInstanceForTab(tab);
}

GlicInstanceImpl* GlicBrowserInteractiveMigrationTest::GetOnlyGlicInstance() {
  return static_cast<GlicInstanceImpl*>(
      ::glic::GetOnlyGlicInstance(browser()->profile()));
}

GlicInstanceImpl* GlicBrowserInteractiveMigrationTest::GetInstanceForTab(
    tabs::TabInterface* tab) {
  return static_cast<GlicInstanceImpl*>(
      ::glic::GetInstanceForTab(browser()->profile(), tab));
}

GlicInstanceImpl* GlicBrowserInteractiveMigrationTest::GetInstanceById(
    InstanceId id) {
  return static_cast<GlicInstanceImpl*>(
      ::glic::GetInstanceById(browser()->profile(), id));
}

GlicInstanceCoordinatorImpl&
GlicBrowserInteractiveMigrationTest::coordinator() {
  return static_cast<GlicInstanceCoordinatorImpl&>(
      GlicKeyedService::Get(browser()->profile())->instance_coordinator());
}

tabs::TabInterface* GlicBrowserInteractiveMigrationTest::CreateAndActivateTab(
    const GURL& url) {
  tabs::TabInterface* new_tab =
      TabListInterface::From(browser())->OpenTab(url, -1);
  TabListInterface::From(browser())->ActivateTab(new_tab->GetHandle());
  CHECK(content::WaitForLoadStop(new_tab->GetContents()));
  return new_tab;
}

TestResult<> GlicBrowserInteractiveMigrationTest::WaitForWebUiState(
    mojom::WebUiState state) {
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

GlicKeyedService* GlicBrowserInteractiveMigrationTest::service() {
  return GlicKeyedService::Get(browser()->profile());
}

BrowserWindowInterface* GlicBrowserInteractiveMigrationTest::GetBrowser() {
  return TabListInterface::From(browser())
      ->GetActiveTab()
      ->GetBrowserWindowInterface();
}

GURL GlicBrowserInteractiveMigrationTest::GetSimpleTestUrl() {
  return GetTestUrl("page.html");
}

GURL GlicBrowserInteractiveMigrationTest::GetTestUrl(
    const std::string& file_name) {
  return embedded_test_server()->GetURL("/test_data/" + file_name);
}

void GlicBrowserInteractiveMigrationTest::SetGlicPagePath(
    const std::string& glic_page_path) {
  glic_test_environment_.SetGlicPagePath(glic_page_path);
}

void GlicBrowserInteractiveMigrationTest::AddMockGlicQueryParam(
    const std::string_view& key,
    const std::string_view& value) {
  glic_test_environment_.AddMockGlicQueryParam(key, value);
}

GURL GlicBrowserInteractiveMigrationTest::GetGuestURL() {
  return glic_test_environment_.GetGuestURL();
}

void GlicBrowserInteractiveMigrationTest::SetGlicFreUrlOverride(
    const GURL& url) {
  glic_test_environment_.SetGlicFreUrlOverride(url);
}

}  // namespace glic
