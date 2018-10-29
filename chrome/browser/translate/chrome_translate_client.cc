// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/translate/translate_accept_languages_factory.h"
#include "chrome/browser/translate/translate_ranker_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/translate/translate_bubble_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/language_detection_logging_helper.h"
#include "components/translate/core/common/translation_logging_helper.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/translate_event.pb.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#endif

namespace {
using base::FeatureList;
using metrics::TranslateEventProto;

TranslateEventProto::EventType BubbleResultToTranslateEvent(
    ShowTranslateBubbleResult result) {
  switch (result) {
    case ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID:
      return TranslateEventProto::BROWSER_WINDOW_IS_INVALID;
    case ShowTranslateBubbleResult::BROWSER_WINDOW_MINIMIZED:
      return TranslateEventProto::BROWSER_WINDOW_IS_MINIMIZED;
    case ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_ACTIVE:
      return TranslateEventProto::BROWSER_WINDOW_NOT_ACTIVE;
    case ShowTranslateBubbleResult::WEB_CONTENTS_NOT_ACTIVE:
      return TranslateEventProto::WEB_CONTENTS_NOT_ACTIVE;
    case ShowTranslateBubbleResult::EDITABLE_FIELD_IS_ACTIVE:
      return TranslateEventProto::EDITABLE_FIELD_IS_ACTIVE;
    default:
      NOTREACHED();
      return metrics::TranslateEventProto::UNKNOWN;
  }
}

// ========== LOG TRANSLATE EVENT ==============

void LogTranslateEvent(const content::WebContents* const web_contents,
                       const metrics::TranslateEventProto& translate_event) {
  if (!FeatureList::IsEnabled(switches::kSyncUserTranslationEvents))
    return;
  DCHECK(web_contents);
  auto* const profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  syncer::UserEventService* const user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile);

  const auto* const entry =
      web_contents->GetController().GetLastCommittedEntry();

  // If entry is null, we don't record the page.
  // The navigation entry can be null in situations like download or initial
  // blank page.
  if (entry == nullptr)
    return;

  auto specifics = std::make_unique<sync_pb::UserEventSpecifics>();
  // We only log the event we care about.
  const bool needs_logging = translate::ConstructTranslateEvent(
      entry->GetTimestamp().ToInternalValue(), translate_event,
      specifics.get());
  if (needs_logging) {
    user_event_service->RecordUserEvent(std::move(specifics));
  }
}

}  // namespace

ChromeTranslateClient::ChromeTranslateClient(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      translate_driver_(&web_contents->GetController()),
      translate_manager_(new translate::TranslateManager(
          this,
          translate::TranslateRankerFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
          LanguageModelManagerFactory::GetForBrowserContext(
              web_contents->GetBrowserContext())
              ->GetPrimaryModel())) {
  translate_driver_.AddObserver(this);
  translate_driver_.set_translate_manager(translate_manager_.get());
}

ChromeTranslateClient::~ChromeTranslateClient() {
  translate_driver_.RemoveObserver(this);
}

translate::LanguageState& ChromeTranslateClient::GetLanguageState() {
  return translate_manager_->GetLanguageState();
}

// static
std::unique_ptr<translate::TranslatePrefs>
ChromeTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
#if defined(OS_CHROMEOS)
  const char* preferred_languages_prefs = prefs::kLanguagePreferredLanguages;
#else
  const char* preferred_languages_prefs = NULL;
#endif
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      new translate::TranslatePrefs(prefs, prefs::kAcceptLanguages,
                                    preferred_languages_prefs));

  // We need to obtain the country here, since it comes from VariationsService.
  // components/ does not have access to that.
  DCHECK(g_browser_process);
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    translate_prefs->SetCountry(
        variations_service->GetStoredPermanentCountry());
  }

  return translate_prefs;
}

// static
translate::TranslateAcceptLanguages*
ChromeTranslateClient::GetTranslateAcceptLanguages(
    content::BrowserContext* browser_context) {
  return TranslateAcceptLanguagesFactory::GetForBrowserContext(browser_context);
}

// static
translate::TranslateManager* ChromeTranslateClient::GetManagerFromWebContents(
    content::WebContents* web_contents) {
  ChromeTranslateClient* chrome_translate_client =
      FromWebContents(web_contents);
  if (!chrome_translate_client)
    return NULL;
  return chrome_translate_client->GetTranslateManager();
}

// static
void ChromeTranslateClient::GetTranslateLanguages(
    content::WebContents* web_contents,
    std::string* source,
    std::string* target) {
  DCHECK(source != NULL);
  DCHECK(target != NULL);

  ChromeTranslateClient* chrome_translate_client =
      FromWebContents(web_contents);
  if (!chrome_translate_client)
    return;

  *source = translate::TranslateDownloadManager::GetLanguageCode(
      chrome_translate_client->GetLanguageState().original_language());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefs(profile->GetPrefs());
  if (!web_contents->GetBrowserContext()->IsOffTheRecord()) {
    std::string auto_translate_language =
        translate::TranslateManager::GetAutoTargetLanguage(
            *source, translate_prefs.get());
    if (!auto_translate_language.empty()) {
      *target = auto_translate_language;
      return;
    }
  }

  *target = translate::TranslateManager::GetTargetLanguage(
      translate_prefs.get(), LanguageModelManagerFactory::GetInstance()
                                 ->GetForBrowserContext(profile)
                                 ->GetPrimaryModel());
}

void ChromeTranslateClient::RecordTranslateEvent(
    const TranslateEventProto& translate_event) {
  LogTranslateEvent(web_contents(), translate_event);
}

translate::TranslateManager* ChromeTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

bool ChromeTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  DCHECK(web_contents());
  DCHECK(translate_manager_);

  if (error_type != translate::TranslateErrors::NONE)
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;

// Translate uses a bubble UI on desktop and an infobar on Android (here)
// and iOS (in ios/chrome/browser/translate/chrome_ios_translate_client.mm).
#if defined(OS_ANDROID)
  if (!TranslateService::IsTranslateBubbleEnabled()) {
    // Infobar UI.
    translate::TranslateInfoBarDelegate::Create(
        step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
        translate_manager_->GetWeakPtr(),
        InfoBarService::FromWebContents(web_contents()),
        web_contents()->GetBrowserContext()->IsOffTheRecord(), step,
        source_language, target_language, error_type, triggered_from_menu);
    return true;
  }
#endif

  // Bubble UI.
  if (step == translate::TRANSLATE_STEP_BEFORE_TRANSLATE &&
      translate_manager_->ShouldSuppressBubbleUI(triggered_from_menu,
                                                 source_language)) {
    return false;
  }

  ShowTranslateBubbleResult result = ShowBubble(step, error_type);
  if (result != ShowTranslateBubbleResult::SUCCESS &&
      step == translate::TRANSLATE_STEP_BEFORE_TRANSLATE) {
    translate_manager_->RecordTranslateEvent(
        BubbleResultToTranslateEvent(result));
  }

  return true;
}

translate::TranslateDriver* ChromeTranslateClient::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* ChromeTranslateClient::GetPrefs() {
  DCHECK(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  return profile->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
ChromeTranslateClient::GetTranslatePrefs() {
  return CreateTranslatePrefs(GetPrefs());
}

translate::TranslateAcceptLanguages*
ChromeTranslateClient::GetTranslateAcceptLanguages() {
  DCHECK(web_contents());
  return GetTranslateAcceptLanguages(web_contents()->GetBrowserContext());
}

#if defined(OS_ANDROID)
int ChromeTranslateClient::GetInfobarIconID() const {
  return IDR_ANDROID_INFOBAR_TRANSLATE;
}
#endif

void ChromeTranslateClient::RecordLanguageDetectionEvent(
    const translate::LanguageDetectionDetails& details) const {
  if (!FeatureList::IsEnabled(switches::kSyncUserLanguageDetectionEvents))
    return;

  DCHECK(web_contents());
  auto* const profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  syncer::UserEventService* const user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile);

  const auto* const entry =
      web_contents()->GetController().GetLastCommittedEntry();

  // If entry is null, we don't record the page.
  // The navigation entry can be null in situations like download or initial
  // blank page.
  if (entry != nullptr &&
      TranslateService::IsTranslatableURL(entry->GetVirtualURL())) {
    user_event_service->RecordUserEvent(
        translate::ConstructLanguageDetectionEvent(
            entry->GetTimestamp().ToInternalValue(), details));
  }
}

bool ChromeTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateService::IsTranslatableURL(url);
}

void ChromeTranslateClient::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
#if defined(OS_ANDROID)
  // Android does not support reporting language detection errors.
  NOTREACHED();
#else
  // We'll open the URL in a new tab so that the user can tell us more.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser) {
    NOTREACHED();
    return;
  }

  chrome::AddSelectedTabWithURL(browser, report_url,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK);
#endif  // defined(OS_ANDROID)
}

void ChromeTranslateClient::WebContentsDestroyed() {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with NULL WebContents.
  translate_manager_.reset();
}

// ContentTranslateDriver::Observer implementation.

void ChromeTranslateClient::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  // TODO(268984): Remove translate notifications and have the clients be
  // ContentTranslateDriver::Observer directly instead.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<const translate::LanguageDetectionDetails>(&details));

  RecordLanguageDetectionEvent(details);
}

void ChromeTranslateClient::OnPageTranslated(
    const std::string& original_lang,
    const std::string& translated_lang,
    translate::TranslateErrors::Type error_type) {
  // TODO(268984): Remove translate notifications and have the clients be
  // ContentTranslateDriver::Observer directly instead.
  DCHECK(web_contents());
  translate::PageTranslatedDetails details;
  details.source_language = original_lang;
  details.target_language = translated_lang;
  details.error_type = error_type;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::Source<content::WebContents>(web_contents()),
      content::Details<translate::PageTranslatedDetails>(&details));
}

ShowTranslateBubbleResult ChromeTranslateClient::ShowBubble(
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type) {
  DCHECK(translate_manager_);
// The bubble is implemented only on the desktop platforms.
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  // |browser| might be NULL when testing. In this case, Show(...) should be
  // called because the implementation for testing is used.
  if (!browser) {
    return TranslateBubbleFactory::Show(NULL, web_contents(), step, error_type);
  }

  if (web_contents() != browser->tab_strip_model()->GetActiveWebContents())
    return ShowTranslateBubbleResult::WEB_CONTENTS_NOT_ACTIVE;

  // This ShowBubble function is also used for updating the existing bubble.
  // However, with the bubble shown, any browser windows are NOT activated
  // because the bubble takes the focus from the other widgets including the
  // browser windows. So it is checked that |browser| is the last activated
  // browser, not is now activated.
  if (browser != chrome::FindLastActive())
    return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_ACTIVE;

  // During auto-translating, the bubble should not be shown.
  if (step == translate::TRANSLATE_STEP_TRANSLATING ||
      step == translate::TRANSLATE_STEP_AFTER_TRANSLATE) {
    if (GetLanguageState().InTranslateNavigation())
      return ShowTranslateBubbleResult::SUCCESS;
  }

  return TranslateBubbleFactory::Show(browser->window(), web_contents(), step,
                                      error_type);
#else
  NOTREACHED();
  return ShowTranslateBubbleResult::SUCCESS;
#endif
}
