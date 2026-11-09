// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reader_mode/reader_mode_service_factory.h"

#include <utility>

#include "base/callback_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/web_distiller/content/browser/distiller_page_web_contents.h"
#include "components/web_distiller/core/article_entry.h"
#include "components/web_distiller/core/distiller.h"
#include "components/web_distiller/core/distiller_options.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/reader_mode/reader_mode_ui_handle_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace reader_mode {

ReaderModeContextKeyedService::ReaderModeContextKeyedService(
    std::unique_ptr<web_distiller::DistillerFactory> distiller_factory,
    std::unique_ptr<web_distiller::DistillerPageFactory> distiller_page_factory,
    std::unique_ptr<web_distiller::DistilledPagePrefs> distilled_page_prefs,
    std::unique_ptr<web_distiller::DistillerUIHandle> distiller_ui_handle,
    base::CallbackListSubscription distilled_page_prefs_subscription)
    : web_distiller::WebDistillerService(std::move(distiller_factory),
                                         std::move(distiller_page_factory),
                                         std::move(distilled_page_prefs),
                                         std::move(distiller_ui_handle)),
      distilled_page_prefs_subscription_(
          std::move(distilled_page_prefs_subscription)) {}

ReaderModeContextKeyedService::~ReaderModeContextKeyedService() = default;

// static
ReaderModeServiceFactory* ReaderModeServiceFactory::GetInstance() {
  static base::NoDestructor<ReaderModeServiceFactory> instance;
  return instance.get();
}

// static
ReaderModeContextKeyedService* ReaderModeServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReaderModeContextKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ReaderModeServiceFactory::ReaderModeServiceFactory()
    : ProfileKeyedServiceFactory(
          "DomDistillerService",
          // Makes normal profile and off-the-record profile use same service
          // instance.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

ReaderModeServiceFactory::~ReaderModeServiceFactory() = default;

std::unique_ptr<KeyedService>
ReaderModeServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  base::FilePath database_dir(
      context->GetPath().Append(FILE_PATH_LITERAL("Articles")));

  std::unique_ptr<web_distiller::DistillerPageFactory> distiller_page_factory(
      new web_distiller::DistillerPageWebContentsFactory(context));
  std::unique_ptr<web_distiller::DistillerFactory> distiller_factory =
      std::make_unique<web_distiller::DistillerFactoryImpl>(
          web_distiller::DistillerOptions());

  std::unique_ptr<web_distiller::DistilledPagePrefs> distilled_page_prefs =
      std::make_unique<web_distiller::DistilledPagePrefs>(profile->GetPrefs());
  distilled_page_prefs->SetDefaultFontScaling(
      profile->GetZoomLevelPrefs()->GetDefaultZoomFactor());
  base::CallbackListSubscription distilled_page_prefs_subscription =
      profile->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
          base::BindRepeating(&ReaderModeServiceFactory::
                                  UpdateDistilledPagePrefsDefaultFontScaling,
                              weak_ptr_factory_.GetWeakPtr(),
                              base::Unretained(context)));
  std::unique_ptr<web_distiller::DistillerUIHandle> distiller_ui_handle;

#if BUILDFLAG(IS_ANDROID)
  distiller_ui_handle =
      std::make_unique<reader_mode::android::ReaderModeUIHandleAndroid>();
#endif  // BUILDFLAG(IS_ANDROID)

  return std::make_unique<ReaderModeContextKeyedService>(
      std::move(distiller_factory), std::move(distiller_page_factory),
      std::move(distilled_page_prefs), std::move(distiller_ui_handle),
      std::move(distilled_page_prefs_subscription));
}

void ReaderModeServiceFactory::UpdateDistilledPagePrefsDefaultFontScaling(
    content::BrowserContext* context) const {
  ReaderModeContextKeyedService* service =
      ReaderModeServiceFactory::GetForBrowserContext(context);
  CHECK(service, base::NotFatalUntil::M161);
  web_distiller::DistilledPagePrefs* distilled_page_prefs =
      service->GetDistilledPagePrefs();

  Profile* profile = Profile::FromBrowserContext(context);
  distilled_page_prefs->SetDefaultFontScaling(
      profile->GetZoomLevelPrefs()->GetDefaultZoomFactor());
}

}  // namespace reader_mode
