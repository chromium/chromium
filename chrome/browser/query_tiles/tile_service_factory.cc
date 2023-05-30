// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/query_tiles/tile_service_factory.h"

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/query_tiles/query_tile_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/language/core/browser/locale_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/query_tiles/tile_service_factory_helper.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/TileServiceUtils_jni.h"
#endif

namespace query_tiles {
namespace {

std::string GetGoogleAPIKey() {
  bool is_stable_channel =
      chrome::GetChannel() == version_info::Channel::STABLE;
  return is_stable_channel ? google_apis::GetAPIKey()
                           : google_apis::GetNonStableAPIKey();
}

}  // namespace

// static
TileServiceFactory* TileServiceFactory::GetInstance() {
  static base::NoDestructor<TileServiceFactory> instance;
  return instance.get();
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

TileServiceFactory::~TileServiceFactory() = default;

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
#if BUILDFLAG(IS_ANDROID)
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
