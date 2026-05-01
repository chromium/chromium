// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_INTERACTIVE_MIGRATION_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_INTERACTIVE_MIGRATION_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test_utils.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/test/mock_activation_controller.h"
#endif

namespace glic {

// TODO(b/508621027): DO NOT USE THIS.
// This test fixture is a temporary to compare flake rates.

class GlicBrowserInteractiveMigrationTest : public InteractiveBrowserTest {
 public:
  GlicBrowserInteractiveMigrationTest();
  ~GlicBrowserInteractiveMigrationTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void ToggleGlicForActiveTab(bool prevent_close = false);
  [[nodiscard]] TestResult<GlicInstanceImpl*> OpenGlicForActiveTab();
  void PreventDeletionOnClose(
      GlicInstanceImpl* instance = nullptr,
      const std::string& conversation_id = "test_conversation");
  void CloseAllEmbeddersAndPreventDeletion(
      GlicInstanceImpl* instance = nullptr);
  [[nodiscard]] TestResult<GlicInstanceImpl*> OpenGlicForActiveTabAndDetach();
  [[nodiscard]] TestResult<GlicInstanceImpl*> WaitForGlicOpen(
      GlicInstance* instance = nullptr);
  [[nodiscard]] TestResult<GlicInstanceImpl*> WaitForGlicOpen(
      tabs::TabInterface* tab);
  TestResult<> WaitForGlicClose(GlicInstance* instance = nullptr);
  TestResult<> CloseGlicForTabAndWait(tabs::TabInterface* tab);
  [[nodiscard]] TestResult<GlicInstanceImpl*> WaitForGlicInstanceBoundToTab(
      tabs::TabInterface* tab);
  GlicInstanceImpl* GetOnlyGlicInstance();
  GlicInstanceImpl* GetInstanceForTab(tabs::TabInterface* tab);
  GlicInstanceImpl* GetInstanceById(InstanceId id);
  GlicInstanceCoordinatorImpl& coordinator();
  tabs::TabInterface* CreateAndActivateTab(const GURL& url);
  [[nodiscard]] TestResult<> WaitForWebUiState(mojom::WebUiState state);
  GlicKeyedService* service();
  BrowserWindowInterface* GetBrowser();
  GURL GetSimpleTestUrl();
  GURL GetTestUrl(const std::string& file_name);
  void SetGlicPagePath(const std::string& glic_page_path);
  void AddMockGlicQueryParam(const std::string_view& key,
                             const std::string_view& value = "");
  GURL GetGuestURL();
  void SetGlicFreUrlOverride(const GURL& url);

 protected:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  std::unique_ptr<views::test::MockActivationController> activation_controller_;
#endif
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_BROWSER_INTERACTIVE_MIGRATION_TEST_H_
