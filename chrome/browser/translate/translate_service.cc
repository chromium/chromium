// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_service.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/language/core/browser/language_model.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/file_manager/app_id.h"
#include "extensions/common/constants.h"
#endif

namespace {
// The singleton instance of TranslateService.
TranslateService* g_translate_service = nullptr;
}  // namespace

TranslateService::TranslateService()
    : resource_request_allowed_notifier_(
          g_browser_process->local_state(),
          switches::kDisableBackgroundNetworking,
          base::BindOnce(&content::GetNetworkConnectionTracker)) {
  resource_request_allowed_notifier_.Init(this, true /* leaky */);
}

TranslateService::~TranslateService() {}

// static
void TranslateService::Initialize() {
  if (g_translate_service)
    return;

  g_translate_service = new TranslateService;
  // Initialize the allowed state for resource requests.
  g_translate_service->OnResourceRequestsAllowed();
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  SystemNetworkContextManager* system_network_context_manager =
      g_browser_process->system_network_context_manager();
  // Manager will be null if called from InitializeForTesting.
  if (system_network_context_manager) {
    download_manager->set_url_loader_factory(
        system_network_context_manager->GetSharedURLLoaderFactory());
  }
  download_manager->set_application_locale(
      g_browser_process->GetApplicationLocale());
}

// static
void TranslateService::Shutdown() {
  translate::TranslateDownloadManager::GetInstance()->Shutdown();
  delete g_translate_service;
  g_translate_service = nullptr;
}

// static
void TranslateService::InitializeForTesting(
    network::mojom::ConnectionType type) {
  if (!g_translate_service) {
    TranslateService::Initialize();
    translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
  } else {
    translate::TranslateDownloadManager::GetInstance()->ResetForTesting();
  }

  g_translate_service->resource_request_allowed_notifier_
      .SetConnectionTypeForTesting(type);
  g_translate_service->OnResourceRequestsAllowed();
}

// static
void TranslateService::ShutdownForTesting() {
  TranslateService::Shutdown();
}

void TranslateService::OnResourceRequestsAllowed() {
  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  language_list->SetResourceRequestsAllowed(
      resource_request_allowed_notifier_.ResourceRequestsAllowed());
}

// static
bool TranslateService::IsTranslateBubbleEnabled() {
#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
  return true;
#else
  // The bubble UX is not implemented on other platforms.
  return false;
#endif
}

// static
std::string TranslateService::GetTargetLanguage(
    PrefService* prefs,
    language::LanguageModel* language_model) {
  return translate::TranslateManager::GetTargetLanguage(
      ChromeTranslateClient::CreateTranslatePrefs(prefs).get(), language_model);
}

// static
bool TranslateService::IsTranslatableURL(const GURL& url) {
  // A URL is translatable unless it is one of the following:
  // - empty (can happen for popups created with window.open(""))
  // - an internal URL:
  //   - chrome:// and chrome-native:// for all platforms
  // - the devtools (which is considered UI)
  // - about:blank
  // - Chrome OS file manager extension
  // Note: Keep in sync with condition in TranslateAgent::PageCaptured.
  return !url.is_empty() && !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(chrome::kChromeNativeScheme) &&
         !url.SchemeIs(content::kChromeDevToolsScheme) &&
#if BUILDFLAG(IS_CHROMEOS_ASH)
         !(url.SchemeIs(extensions::kExtensionScheme) &&
           url.DomainIs(file_manager::kFileManagerAppId)) &&
#endif
         !url.IsAboutBlank();
}

bool TranslateService::IsAvailable(PrefService* prefs) {
  return translate::TranslateManager::IsAvailable(
      ChromeTranslateClient::CreateTranslatePrefs(prefs).get());
}
