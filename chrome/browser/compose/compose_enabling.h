// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "chrome/browser/compose/proto/compose_optimization_guide.pb.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill/core/common/aliases.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/optimization_guide/core/model_execution/settings_enabled_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"

namespace compose {

enum class ComposeNudgeDenyReason {
  kSavedStateNotificationDisabled = 0,
  kSavedStateNudgeDisabled = 1,
  // The proactive nudge could have shown but was disabled by preference or
  // config values.
  kProactiveNudgeDisabled = 2,
  // The proactive nudge can not be shown for this user, page, or field.
  kProactiveNudgeBlocked = 3,
};

}  // namespace compose

class ComposeEnabling {
 public:
  using ScopedOverride = std::unique_ptr<base::ScopedClosureRunner>;

  explicit ComposeEnabling(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      OptimizationGuideKeyedService* opt_guide);
  ~ComposeEnabling();

  ComposeEnabling(const ComposeEnabling&) = delete;
  ComposeEnabling& operator=(const ComposeEnabling&) = delete;

  // Static method that verifies that the feature can be enabled on the given
  // profile. Doesn't take advantage of for test opt_guide or identity_manager,
  // use member function version if you need to mock them out.
  static bool IsEnabledForProfile(Profile* profile);

  // Instance method that verifies that the feature can be enabled for the
  // profile associated with this instance upon construction.
  base::expected<void, compose::ComposeShowStatus> IsEnabled();

  // The following methods allow overriding is-enabled checks to facilitate
  // testing. The returned instances must be kept in scope while the override
  // is required. When they go out of scope, the override is reverted. If
  // multiple overrides are created, the override is reverted only when all
  // instances are destroyed. These implementations are not multi-thread safe.
  static ScopedOverride ScopedEnableComposeForTesting();
  static ScopedOverride ScopedSkipUserCheckForTesting();
  // VariationsService is not available for unit tests, hence the need for this
  // override method. Browser tests are recommended to use
  // VariationsService::OverrideStoredPermanentCountry because it exercises a
  // code path closer to the runtime one.
  static ScopedOverride OverrideCountryForTesting(std::string country_code);

  base::expected<void, compose::ComposeShowStatus> ShouldTriggerNoStatePopup(
      std::string_view autocomplete_attribute,
      bool writingsuggestions_attribute,
      Profile* profile,
      PrefService* prefs,
      translate::TranslateManager* translate_manager,
      const url::Origin& top_level_frame_origin,
      const url::Origin& element_frame_origin,
      GURL url,
      bool is_msbb_enabled);

  bool ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource trigger_source);

  bool ShouldTriggerContextMenu(Profile* profile,
                                translate::TranslateManager* translate_manager,
                                content::RenderFrameHost* rfh,
                                content::ContextMenuParams& params);

  compose::ComposeHintDecision GetOptimizationGuidanceForUrl(const GURL& url,
                                                             Profile* profile);

  // Checks if the page assessed language is supported by Compose.
  bool IsPageLanguageSupported(translate::TranslateManager* translate_manager);

 private:
  base::expected<void, compose::ComposeShowStatus> PageLevelChecks(
      translate::TranslateManager* translate_manager,
      GURL url,
      const url::Origin& top_level_frame_origin,
      const url::Origin& element_frame_origin,
      bool is_newsted_within_fenced_frame);

  static base::expected<void, compose::ComposeShowStatus> CheckEnabling(
      OptimizationGuideKeyedService* opt_guide,
      signin::IdentityManager* identity_manager);

  raw_ptr<Profile> profile_;
  raw_ptr<OptimizationGuideKeyedService> opt_guide_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  static int enabled_for_testing_;
  static int skip_user_check_for_testing_;
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
