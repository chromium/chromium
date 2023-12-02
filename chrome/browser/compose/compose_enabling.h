// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_

#include "base/types/expected.h"
#include "chrome/browser/compose/proto/compose_optimization_guide.pb.h"
#include "chrome/browser/compose/translate_language_provider.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/compose/core/browser/compose_metrics.h"
#include "components/optimization_guide/core/model_execution/settings_enabled_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"

class ComposeEnabling : public optimization_guide::SettingsEnabledObserver {
 public:
  explicit ComposeEnabling(
      TranslateLanguageProvider* translate_language_provider,
      Profile* profile);
  ~ComposeEnabling() override;

  ComposeEnabling(const ComposeEnabling&) = delete;
  ComposeEnabling& operator=(const ComposeEnabling&) = delete;

  base::expected<void, compose::ComposeShowStatus> IsEnabledForProfile(
      Profile* profile);
  base::expected<void, compose::ComposeShowStatus> IsEnabled(
      Profile* profile,
      signin::IdentityManager* identity_manager);
  void SetEnabledForTesting();
  void ClearEnabledForTesting();
  void SkipUserEnabledCheckForTesting(bool skip);
  bool ShouldTriggerPopup(std::string_view autocomplete_attribute,
                          Profile* profile,
                          translate::TranslateManager* translate_manager,
                          bool ongoing_session,
                          const url::Origin& top_level_frame_origin,
                          const url::Origin& element_frame_origin,
                          GURL url);
  bool ShouldTriggerContextMenu(Profile* profile,
                                translate::TranslateManager* translate_manager,
                                content::RenderFrameHost* rfh,
                                content::ContextMenuParams& params);

  compose::ComposeHintDecision GetOptimizationGuidanceForUrl(const GURL& url,
                                                             Profile* profile);

  // SettingsEnabledObserver implementation
  // TODO(b/314201066): This should be moved to another class that is
  // instantiated once per-profile.
  void PrepareToEnableOnRestart() override;

 private:
  raw_ptr<TranslateLanguageProvider> translate_language_provider_;
  raw_ptr<Profile> profile_;
  raw_ptr<OptimizationGuideKeyedService> opt_guide_;
  bool enabled_for_testing_{false};
  bool skip_user_check_for_testing_{false};

  base::expected<void, compose::ComposeShowStatus> PageLevelChecks(
      Profile* profile,
      translate::TranslateManager* translate_manager,
      const url::Origin& top_level_frame_origin,
      const url::Origin& element_frame_origin);
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
