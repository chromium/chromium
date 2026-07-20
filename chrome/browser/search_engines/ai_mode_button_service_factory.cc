// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/ai_mode_button_service_factory.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/ai_mode_button_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
AiModeButtonService* AiModeButtonServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AiModeButtonService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AiModeButtonServiceFactory* AiModeButtonServiceFactory::GetInstance() {
  static base::NoDestructor<AiModeButtonServiceFactory> instance;
  return instance.get();
}

AiModeButtonServiceFactory::AiModeButtonServiceFactory()
    : ProfileKeyedServiceFactory(
          "AiModeButtonService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

AiModeButtonServiceFactory::~AiModeButtonServiceFactory() = default;

std::unique_ptr<KeyedService>
AiModeButtonServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return nullptr;
  }

  // TODO(crbug.com/517976551): Service is in components/ and can't access
  //   chrome strings. For now, just wire them in from the factory.
  AiModeButtonService::GoogleStrings google_strings;
  google_strings.entrypoint_label =
      l10n_util::GetStringUTF16(IDS_AI_MODE_ENTRYPOINT_LABEL);
#if BUILDFLAG(IS_ANDROID)
  google_strings.context_menu_label = u"";
#else
  google_strings.context_menu_label =
      l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SHOW_AI_MODE_OMNIBOX_BUTTON);
#endif

  return std::make_unique<AiModeButtonService>(template_url_service,
                                               google_strings);
}
