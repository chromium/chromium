// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERACTIVE_TEST_BASE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERACTIVE_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_interactive_test_base.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class Profile;

namespace contextual_tasks {

inline constexpr char kMockAimPagePath[] =
    "chrome/test/data/mock_aim_page.html";
inline constexpr char kMockAimPageHost[] = "www.google.com";

// Test TabContextualizationController providing deterministic screenshot
// bitmaps.
class TestTabContextualizationController
    : public lens::TabContextualizationController {
 public:
  static SkColor screenshot_color_;

  explicit TestTabContextualizationController(tabs::TabInterface* tab);
  ~TestTabContextualizationController() override;

  void CaptureScreenshot(
      std::optional<lens::ImageEncodingOptions> image_options,
      CaptureScreenshotCallback callback) override;

 protected:
  bool IsPageContextEligible(
      const GURL& url,
      const std::vector<optimization_guide::FrameMetadata>& frame_metadata)
      override;
};

// Mock eligibility manager returning eligibility based on sign-in state.
class TestContextualTasksEligibilityManager
    : public ContextualTasksEligibilityManager {
 public:
  TestContextualTasksEligibilityManager(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service,
      bool is_signed_in = true);
  ~TestContextualTasksEligibilityManager() override;

  bool IsEligibleWithoutIdentity() const override;
  bool CalculateEligibility() const override;

 private:
  bool is_signed_in_ = true;
};

// Test ContextualTasksUiService with sign-in state control.
class TestContextualTasksUiService : public ContextualTasksUiService {
 public:
  TestContextualTasksUiService(Profile* profile,
                               ContextualTasksService* contextual_tasks_service,
                               AimEligibilityService* aim_eligibility_service,
                               signin::IdentityManager* identity_manager,
                               bool is_signed_in = true);
  ~TestContextualTasksUiService() override;

  bool IsSignedInToBrowserWithValidCredentials() override;
  bool CookieJarContainsPrimaryAccount() override;
  bool IsUrlForPrimaryAccount(const GURL& url) override;
  void SetSignedIn(bool is_signed_in);
  void GetAccessToken(
      GetAccessTokenCallback callback,
      base::WeakPtr<content::WebContents> web_contents) override;

 private:
  bool is_signed_in_ = true;
};

// Base class for Contextual Tasks interactive UI tests, inheriting from
// LensOverlayInteractiveTestBase.
class ContextualTasksInteractiveTestBase
    : public LensOverlayInteractiveTestBase {
 public:
  template <typename... Args>
  explicit ContextualTasksInteractiveTestBase(Args&&... args)
      : LensOverlayInteractiveTestBase(std::forward<Args>(args)...) {
    InitTabContextOverride();
  }
  ~ContextualTasksInteractiveTestBase() override;

  static std::vector<base::test::FeatureRefAndParams>
  GetDefaultEnabledFeatures();
  static std::vector<base::test::FeatureRef> GetDefaultDisabledFeatures();

  void SetUpFeatureList() override;
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  virtual std::unique_ptr<KeyedService> BuildMockAimServiceInstance(
      content::BrowserContext* context);
  virtual std::unique_ptr<KeyedService>
  BuildMockContextualTasksUiServiceInstance(content::BrowserContext* context);

  MockAimEligibilityService* GetMockAimEligibilityService(Profile* profile);
  TestContextualTasksUiService* GetTestContextualTasksUiService(
      Profile* profile);

 protected:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;

 private:
  void InitTabContextOverride();

  ui::UserDataFactory::ScopedOverride tab_context_override_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERACTIVE_TEST_BASE_H_
