// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/query_tiles/tile_service_factory.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/language/core/browser/locale_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/query_tiles/switches.h"
#include "components/query_tiles/tile_service_factory_helper.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/TileServiceUtils_jni.h"
#endif

namespace query_tiles {
namespace {

// Issue 1076964: Currently the variation service can be only reached in full
// browser mode. Ensure the fetcher task launches OnFullBrowserLoaded.
// TODO(hesen): Work around store/get country code in reduce mode.
std::string GetCountryCode() {
  std::string country_code;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(query_tiles::switches::kQueryTilesCountryCode)) {
    country_code = command_line->GetSwitchValueASCII(
        query_tiles::switches::kQueryTilesCountryCode);
    if (!country_code.empty())
      return country_code;
  }

  if (!g_browser_process)
    return country_code;

  auto* variations_service = g_browser_process->variations_service();
  if (variations_service) {
    country_code = variations_service->GetStoredPermanentCountry();
    if (!country_code.empty())
      return country_code;
    country_code = variations_service->GetLatestCountry();
  }
  return country_code;
}

std::string GetGoogleAPIKey() {
  bool is_stable_channel =
      chrome::GetChannel() == version_info::Channel::STABLE;
  return is_stable_channel ? google_apis::GetAPIKey()
                           : google_apis::GetNonStableAPIKey();
}

}  // namespace

// static
TileServiceFactory* TileServiceFactory::GetInstance() {
  return base::Singleton<TileServiceFactory>::get();
}

// static
TileService* TileServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<TileService*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

TileServiceFactory::TileServiceFactory()
    : SimpleKeyedServiceFactory("TileService",
                                SimpleDependencyManager::GetInstance()) {
  DependsOn(ImageFetcherServiceFactory::GetInstance());
  DependsOn(background_task::BackgroundTaskSchedulerFactory::GetInstance());
}

TileServiceFactory::~TileServiceFactory() {}

std::unique_ptr<KeyedService> TileServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  auto* image_fetcher_service = ImageFetcherServiceFactory::GetForKey(key);
  auto* db_provider =
      ProfileKey::FromSimpleFactoryKey(key)->GetProtoDatabaseProvider();
  // |storage_dir| is not actually used since we are using the shared leveldb.
  base::FilePath storage_dir =
      ProfileKey::FromSimpleFactoryKey(key)->GetPath().Append(
          chrome::kQueryTileStorageDirname);

  auto* background_task_scheduler =
      background_task::BackgroundTaskSchedulerFactory::GetForKey(key);

  std::string accept_languanges =
      ProfileKey::FromSimpleFactoryKey(key)->GetPrefs()->GetString(
          language::prefs::kAcceptLanguages);

  auto url_loader_factory =
      SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();

  base::Version version = version_info::GetVersion();
  std::string channel_name =
      chrome::GetChannelName(chrome::WithExtendedStable(true));
  std::string client_version =
      base::StringPrintf("%d.%d.%d.%s.chrome",
                         version.components()[0],  // Major
                         version.components()[2],  // Build
                         version.components()[3],  // Patch
                         channel_name.c_str());

  std::string default_server_url;
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_server_url =
      Java_TileServiceUtils_getDefaultServerUrl(env);
  default_server_url =
      base::android::ConvertJavaStringToUTF8(env, j_server_url);
#endif
  return CreateTileService(image_fetcher_service, db_provider, storage_dir,
                           background_task_scheduler, accept_languanges,
                           GetCountryCode(), GetGoogleAPIKey(), client_version,
                           default_server_url, url_loader_factory,
                           ProfileKey::FromSimpleFactoryKey(key)->GetPrefs());
}

}  // namespace query_tiles
