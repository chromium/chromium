// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"

#include <memory>

#include "android_webview/common/aw_paths.h"
#include "android_webview/nonembedded/component_updater/aw_component_updater_configurator.h"
#include "android_webview/nonembedded/component_updater/registration.h"
#include "android_webview/nonembedded/nonembedded_jni_headers/AwComponentUpdateService_jni.h"
#include "android_webview/nonembedded/webview_apk_process.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/timer_update_scheduler.h"

namespace android_webview {

namespace {

AwComponentUpdateService* g_aw_component_update_service_for_testing = nullptr;

}  // namespace

// static
AwComponentUpdateService* AwComponentUpdateService::GetInstance() {
  if (g_aw_component_update_service_for_testing) {
    return g_aw_component_update_service_for_testing;
  }
  static base::NoDestructor<AwComponentUpdateService> instance;
  return instance.get();
}

void SetAwComponentUpdateServiceForTesting(AwComponentUpdateService* service) {
  g_aw_component_update_service_for_testing = service;
}

AwComponentUpdateService::AwComponentUpdateService() = default;

AwComponentUpdateService::~AwComponentUpdateService() = default;

bool AwComponentUpdateService::NotifyNewVersion(
    const std::string& component_id,
    const base::FilePath& install_dir,
    const base::Version& version) {
  // TODO(crbug.com/1171771) notify ComponentProviderService about the new
  // version.
  return false;
}

// Start ComponentUpdateService once.
void AwComponentUpdateService::MaybeStartComponentUpdateService() {
  if (cus_)
    return;

  PrefService* pref_service =
      WebViewApkProcess::GetInstance()->GetPrefService();

  // All dirs point to the webview component root dir. Has to be called after
  // init WebViewApkProcess, should only happen once per during startup.
  component_updater::RegisterPathProvider(
      /*components_system_root_key=*/android_webview::DIR_COMPONENTS_ROOT,
      /*components_system_root_key_alt=*/android_webview::DIR_COMPONENTS_ROOT,
      /*components_user_root_key=*/android_webview::DIR_COMPONENTS_ROOT);

  // TODO(crbug.com/1175051): stabilize and use BackgroundTaskUpdateScheduler
  // instead.
  cus_ = component_updater::ComponentUpdateServiceFactory(
      MakeAwComponentUpdaterConfigurator(base::CommandLine::ForCurrentProcess(),
                                         pref_service),
      std::make_unique<component_updater::TimerUpdateScheduler>());

  DCHECK(cus_);
  RegisterComponentsForUpdate(cus_.get());
}

// static
void JNI_AwComponentUpdateService_MaybeStartComponentUpdateService(
    JNIEnv* env) {
  AwComponentUpdateService::GetInstance()->MaybeStartComponentUpdateService();
}

}  // namespace android_webview