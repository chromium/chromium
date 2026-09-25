// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERACTIVE_TEST_BASE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERACTIVE_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/chrome_aim_eligibility_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_test_user_variation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
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

  virtual UserVariation GetUserVariation() const;

  static std::vector<base::test::FeatureRefAndParams>
  GetDefaultEnabledFeatures();
  static std::vector<base::test::FeatureRef> GetDefaultDisabledFeatures();

  void SetUpCommandLine(base::CommandLine* command_line) override;
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

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_
               ? identity_test_env_adaptor_->identity_test_env()
               : nullptr;
  }

 protected:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

 private:
  void InitTabContextOverride();

  ui::UserDataFactory::ScopedOverride tab_context_override_;
};

namespace internal {
template <typename T, typename = void>
struct HasTupleGet : std::false_type {};

template <typename T>
struct HasTupleGet<T, std::void_t<decltype(std::get<0>(std::declval<T>()))>>
    : std::true_type {};
}  // namespace internal

// Parameterized wrapper template that extracts UserVariation from ParamType.
template <typename ParamType>
class ContextualTasksInteractiveTestBaseT
    : public ContextualTasksInteractiveTestBase,
      public testing::WithParamInterface<ParamType> {
 public:
  template <typename... Args>
  explicit ContextualTasksInteractiveTestBaseT(Args&&... args)
      : ContextualTasksInteractiveTestBase(std::forward<Args>(args)...) {}

  UserVariation GetUserVariation() const override {
    if constexpr (std::is_same_v<ParamType, UserVariation>) {
      return this->GetParam();
    } else if constexpr (internal::HasTupleGet<ParamType>::value) {
      return std::get<0>(this->GetParam());
    } else {
      return UserVariation::kSignedIn;
    }
  }
};

}  // namespace contextual_tasks

// Convenience macros for skipping tests based on GetUserVariation().
// These must be macros rather than member functions because GTEST_SKIP()
// contains a 'return;' statement that must return from the caller's test body.
#define SkipIf(target, reason) SKIP_IF(GetUserVariation(), (target), (reason))
#define SkipIfIncognito(reason) SKIP_IF_INCOGNITO(GetUserVariation(), (reason))
#define SkipIfSignedOut(reason) SKIP_IF_SIGNED_OUT(GetUserVariation(), (reason))
#define SkipIfSignedIn(reason) SKIP_IF_SIGNED_IN(GetUserVariation(), (reason))
#define SkipIfNotSignedIn(reason) \
  SKIP_IF_NOT_SIGNED_IN(GetUserVariation(), (reason))

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_INTERACTIVE_TEST_BASE_H_
