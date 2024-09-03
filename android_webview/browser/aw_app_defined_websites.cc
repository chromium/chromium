// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_app_defined_websites.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AppDefinedDomains_jni.h"

namespace android_webview {

namespace {

std::vector<std::string> GetAppDefinedDomainsFromManifest(
    AppDefinedDomainCriteria criteria) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> jobject_domains;
  switch (criteria) {
    case AppDefinedDomainCriteria::kNone: {
      return std::vector<std::string>();
    }
    case AppDefinedDomainCriteria::kAndroidAssetStatements: {
      jobject_domains =
          Java_AppDefinedDomains_getDomainsFromAssetStatements(env);
      break;
    }
    case AppDefinedDomainCriteria::kAndroidVerifiedAppLinks: {
      jobject_domains =
          Java_AppDefinedDomains_getVerifiedDomainsFromAppLinks(env);
      break;
    }
    case AppDefinedDomainCriteria::kAndroidWebLinks: {
      jobject_domains = Java_AppDefinedDomains_getDomainsFromWebLinks(env);
      break;
    }
    case AppDefinedDomainCriteria::kAndroidAssetStatementsAndWebLinks: {
      jobject_domains =
          Java_AppDefinedDomains_getDomainsFromAssetStatementsAndWebLinks(env);
      break;
    }
    default: {
      return std::vector<std::string>();
    }
  }
  std::vector<std::string> domains;
  base::android::AppendJavaStringArrayToStringVector(env, jobject_domains,
                                                     &domains);
  return domains;
}

}  // namespace

// static
AppDefinedWebsites* AppDefinedWebsites::GetInstance() {
  static base::NoDestructor<AppDefinedWebsites> instance(
      base::BindRepeating(&GetAppDefinedDomainsFromManifest));
  return instance.get();
}

AppDefinedWebsites::AppDefinedWebsites(AppDomainProvider provider)
    : provider_(std::move(provider)) {}
AppDefinedWebsites::~AppDefinedWebsites() = default;

void AppDefinedWebsites::GetAppDefinedDomains(AppDefinedDomainCriteria criteria,
                                              AppDomainCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto find_it = domains_cache_.find(criteria);
  if (find_it != domains_cache_.end()) {
    std::move(callback).Run(*find_it->second);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(provider_, criteria),
      base::BindOnce(&AppDefinedWebsites::DomainsReturnedFromManifest,
                     weak_ptr_factory_.GetWeakPtr(), criteria,
                     std::move(callback)));
}

void AppDefinedWebsites::DomainsReturnedFromManifest(
    AppDefinedDomainCriteria criteria,
    AppDomainCallback callback,
    const std::vector<std::string>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  domains_cache_[criteria] = std::make_unique<std::vector<std::string>>(data);
  std::move(callback).Run(*domains_cache_[criteria]);
}

}  // namespace android_webview
