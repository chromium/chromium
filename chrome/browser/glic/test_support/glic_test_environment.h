// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_ENVIRONMENT_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_ENVIRONMENT_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"

class Profile;
namespace glic {
class GlicKeyedService;
namespace internal {
class TestCookieSynchronizer;
}  // namespace internal
namespace prefs {
enum class FreStatus;
}  // namespace prefs

class GlicTestEnvironmentService;

// Configuration of GlicTestEnvironment.
struct GlicTestEnvironmentConfig {
  // If enabled, forces sign-in and enables model execution capability, which
  // are prerequisites for using Glic.
  bool force_signin_and_model_execution_capability = true;
  // The default FRE status saved to prefs after profile creation.
  std::optional<prefs::FreStatus> fre_status = prefs::FreStatus::kCompleted;
};

// Overrides some glic functionality to allow tests that depend on glic to run.
// This should be created on the main thread.
// If possible, use InteractiveGlicTest instead of this directly!
// This class is used by tests in browser_tests and interactive_ui_tests that
// cannot use InteractiveGlicTest.
//
// Ensures a GlicTestEnvironmentService is created for each browser context, and
// sets the default configuration.
class GlicTestEnvironment {
 public:
  explicit GlicTestEnvironment(
      const GlicTestEnvironmentConfig& config = {},
      std::vector<base::test::FeatureRef> enabled_features =
          {features::kGlic, features::kTabstripComboButton,
           features::kGlicRollout},
      std::vector<base::test::FeatureRef> disabled_features = {
          features::kGlicWarming, features::kGlicFreWarming});

  ~GlicTestEnvironment();

  // Functions to override configuration after creation. These affect only
  // subsequently created profiles.

  // Updates `force_signin_and_model_execution_capability`.
  void SetForceSigninAndModelExecutionCapability(bool force);

  // Sets the `prefs::FreStatus`. If nullopt, keeps the
  // default pref state (`FreStatus::kNotStarted`).
  void SetFreStatusForNewProfiles(std::optional<prefs::FreStatus> fre_status);

  static GlicTestEnvironmentService* GetService(Profile* profile,
                                                bool create = true);

 private:
  void OnWillCreateBrowserContextKeyedServices(
      content::BrowserContext* context);
  base::CallbackListSubscription create_services_subscription_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Note: This constructs the GlicKeyedService, if it's not already created,
// which will also construct dependencies like IdentityManager. You likely want
// to create GlicTestEnvironmentService only after other test environment
// classes, like IdentityTestEnvironmentProfileAdaptor.
class GlicTestEnvironmentService : public KeyedService {
 public:
  explicit GlicTestEnvironmentService(Profile* profile);
  ~GlicTestEnvironmentService() override;

  // Convenience functions.
  void SetFRECompletion(prefs::FreStatus fre_status);
  GlicKeyedService* GetService();

  // Glic syncs sign-in cookies to the webview before showing the window. By
  // default, this class replaces this step with an immediately fake success.
  // Change the result of this operation here.
  void SetResultForFutureCookieSync(bool result);
  void SetResultForFutureCookieSyncInFre(bool result);

 private:
  raw_ptr<Profile> profile_;
  // Null during teardown.
  base::WeakPtr<internal::TestCookieSynchronizer> cookie_synchronizer_;
  base::WeakPtr<internal::TestCookieSynchronizer> fre_cookie_synchronizer_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_GLIC_TEST_ENVIRONMENT_H_
