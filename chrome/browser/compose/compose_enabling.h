// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
#define CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_

#include "chrome/browser/compose/translate_language_provider.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"

class ComposeEnabling {
 public:
  explicit ComposeEnabling(
      TranslateLanguageProvider* translate_language_provider);
  ~ComposeEnabling();
  bool IsEnabledForProfile(Profile* profile);
  bool IsEnabled(Profile* profile, signin::IdentityManager* identity_manager);
  void SetEnabledForTesting();
  void ClearEnabledForTesting();
  std::string GetLanguage();
  bool ShouldTriggerPopup(std::string_view autocomplete_attribute,
                          Profile* profile,
                          translate::TranslateManager* translate_manager,
                          bool has_saved_state);
  bool ShouldTriggerContextMenu(Profile* profile,
                                translate::TranslateManager* translate_manager,
                                content::RenderFrameHost* rfh,
                                content::ContextMenuParams& params);

 private:
  raw_ptr<TranslateLanguageProvider> translate_language_provider_;
  bool enabled_for_testing_;

  bool PageLevelChecks(Profile* profile,
                       translate::TranslateManager* translate_manager);
};

#endif  // CHROME_BROWSER_COMPOSE_COMPOSE_ENABLING_H_
