// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/subkey_requester_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/android/jni_headers/SubKeyRequesterFactory_jni.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

// static
SubKeyRequester* SubKeyRequesterFactory::GetInstance() {
  static base::LazyInstance<SubKeyRequesterFactory>::DestructorAtExit instance =
      LAZY_INSTANCE_INITIALIZER;
  return &(instance.Get().subkey_requester_);
}

SubKeyRequesterFactory::SubKeyRequesterFactory()
    : subkey_requester_(
          std::make_unique<ChromeMetadataSource>(
              I18N_ADDRESS_VALIDATION_DATA_URL,
              g_browser_process->system_network_context_manager()
                  ->GetSharedURLLoaderFactory()),
          ValidationRulesStorageFactory::CreateStorage(),
          l10n_util::GetLanguage(g_browser_process->GetApplicationLocale())) {}

SubKeyRequesterFactory::~SubKeyRequesterFactory() {}

#if BUILDFLAG(IS_ANDROID)
static base::android::ScopedJavaLocalRef<jobject>
JNI_SubKeyRequesterFactory_GetInstance(JNIEnv* env) {
  return SubKeyRequesterFactory::GetInstance()->GetJavaObject();
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill
