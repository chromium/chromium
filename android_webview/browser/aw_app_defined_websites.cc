// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_app_defined_websites.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include "android_webview/browser/aw_asset_domain_list_include_handler.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/barrier_callback.h"
#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AppDefinedDomains_jni.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace android_webview {

namespace {

inline void RemoveDuplicates(std::vector<std::string>& string_vec) {
  // Remove duplicates following the example on
  // https://en.cppreference.com/w/cpp/algorithm/unique.
  std::sort(string_vec.begin(), string_vec.end());
  auto last_unique = std::unique(string_vec.begin(), string_vec.end());
  string_vec.erase(last_unique, string_vec.end());
}

std::vector<std::string> GetAppDefinedDomainsFromManifest(
    AppDefinedDomainCriteria criteria) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> jobject_domains;
  switch (criteria) {
    case AppDefinedDomainCriteria::kNone: {
      return {};
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
      return {};
    }
  }
  std::vector<std::string> domains;
  base::android::AppendJavaStringArrayToStringVector(env, jobject_domains,
                                                     &domains);
  RemoveDuplicates(domains);
  return domains;
}

std::vector<std::string> GetAppDefinedIncludeLInksFromManifest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> jobject_links =
      Java_AppDefinedDomains_getIncludeLinksFromAssetStatements(env);
  std::vector<std::string> links;
  base::android::AppendJavaStringArrayToStringVector(env, jobject_links,
                                                     &links);
  RemoveDuplicates(links);
  return links;
}
}  // namespace

// static
AppDefinedWebsites* AppDefinedWebsites::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<AppDefinedWebsites> instance(
      base::BindRepeating(&GetAppDefinedDomainsFromManifest),
      base::BindRepeating(&GetAppDefinedIncludeLInksFromManifest));
  return instance.get();
}

AppDefinedWebsites::AppDefinedWebsites(
    AppDomainProvider provider,
    IncludeLinkProvider include_link_provider)
    : provider_(std::move(provider)),
      include_link_provider_(std::move(include_link_provider)) {}
AppDefinedWebsites::~AppDefinedWebsites() = default;

void AppDefinedWebsites::GetAppDefinedDomains(AppDefinedDomainCriteria criteria,
                                              AppDomainCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto find_it = domains_cache_.find(criteria);
  if (find_it != domains_cache_.end()) {
    std::move(callback).Run(*find_it->second);
    return;
  }

  AppDomainCallbackList& callback_list = GetCallbackList(criteria);
  bool ongoing_request = !callback_list.empty();
  callback_list.AddUnsafe(std::move(callback));
  if (ongoing_request) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(provider_, criteria),
      base::BindOnce(&AppDefinedWebsites::DomainsReturnedFromManifest,
                     weak_ptr_factory_.GetWeakPtr(), criteria));
}

void AppDefinedWebsites::GetAssetStatmentsWithIncludes(
    std::unique_ptr<AssetDomainListIncludeHandler> domain_list_loader,
    AppDomainSetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (asset_statements_with_includes_) {
    std::move(callback).Run(*asset_statements_with_includes_);
    return;
  }

  bool is_loading = !asset_statements_with_includes_callbacks_.empty();
  asset_statements_with_includes_callbacks_.AddUnsafe(std::move(callback));

  if (is_loading) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(include_link_provider_),
      base::BindOnce(&AppDefinedWebsites::AssetIncludeStatementsReturned,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(domain_list_loader)));
}

void AppDefinedWebsites::AppDeclaresDomainInAssetStatements(
    std::unique_ptr<AssetDomainListIncludeHandler> domain_list_loader,
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  AppDomainCallback callback_wrapper = base::BindOnce(
      [](const url::Origin origin,
         base::OnceCallback<void(bool)> defined_callback,
         const std::vector<std::string>& domains) {
        bool is_defined = std::find_if(domains.begin(), domains.end(),
                                       [&origin](const std::string& domain) {
                                         return origin.DomainIs(domain);
                                       }) != domains.end();
        std::move(defined_callback).Run(is_defined);
      },
      origin, std::move(callback));
  if (base::FeatureList::IsEnabled(
          features::kWebViewDigitalAssetLinksLoadIncludes)) {
    GetAssetStatmentsWithIncludes(std::move(domain_list_loader),
                                  std::move(callback_wrapper));
  } else {
    GetAppDefinedDomains(AppDefinedDomainCriteria::kAndroidAssetStatements,
                         std::move(callback_wrapper));
  }
}

namespace {
// Wrap in a callback that takes a const ref and simply makes a copy.
inline AppDefinedWebsites::AppDomainCallback AsAppDomainCallback(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(std::vector<std::string>)> callback,
         const std::vector<std::string>& data_ref) {
        std::move(callback).Run(data_ref);
      },
      std::move(callback));
}
}  // namespace

void AppDefinedWebsites::AssetIncludeStatementsReturned(
    std::unique_ptr<AssetDomainListIncludeHandler> domain_list_loader,
    std::vector<std::string> include_urls) {
  // The barrier callback should be called once to load domains directly from
  // the manifest, and once per include_url.
  int call_count = 1 + include_urls.size();

  // Grab a direct pointer to call methods on the loader, then move it
  // into the barrier callback to keep it alive until all assets have been
  // loaded.
  AssetDomainListIncludeHandler* raw_loader = domain_list_loader.get();
  base::RepeatingCallback<void(std::vector<std::string>)> barrier_callback =
      base::BarrierCallback<std::vector<std::string>>(
          call_count,
          base::BindOnce(
              &AppDefinedWebsites::OnAssetStatementsWithIncludesLoaded,
              weak_ptr_factory_.GetWeakPtr(), std::move(domain_list_loader)));

  GetAppDefinedDomains(AppDefinedDomainCriteria::kAndroidAssetStatements,
                       AsAppDomainCallback(barrier_callback));

  for (const std::string& include_url : include_urls) {
    raw_loader->LoadAppDefinedDomainIncludes(
        GURL(include_url), AsAppDomainCallback(barrier_callback));
  }
}

AppDefinedWebsites::AppDomainCallbackList& AppDefinedWebsites::GetCallbackList(
    AppDefinedDomainCriteria criteria) {
  auto callback_list_it = on_domains_returned_callbacks_.find(criteria);
  if (callback_list_it == on_domains_returned_callbacks_.end() ||
      !callback_list_it->second) {
    callback_list_it =
        on_domains_returned_callbacks_
            .insert_or_assign(criteria,
                              std::make_unique<AppDomainCallbackList>())
            .first;
  }
  return *callback_list_it->second;
}

void AppDefinedWebsites::DomainsReturnedFromManifest(
    AppDefinedDomainCriteria criteria,
    const std::vector<std::string>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  domains_cache_[criteria] = std::make_unique<std::vector<std::string>>(data);

  AppDomainCallbackList& callback_list = GetCallbackList(criteria);
  callback_list.Notify(*domains_cache_[criteria]);
  DCHECK(callback_list.empty());
}

void AppDefinedWebsites::OnAssetStatementsWithIncludesLoaded(
    std::unique_ptr<AssetDomainListIncludeHandler> domain_list_handler,
    std::vector<std::vector<std::string>> all_domains) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  asset_statements_with_includes_ =
      std::make_unique<std::vector<std::string>>();
  for (std::vector<std::string>& domain_list : all_domains) {
    asset_statements_with_includes_->insert(
        asset_statements_with_includes_->end(),
        std::make_move_iterator(domain_list.begin()),
        std::make_move_iterator(domain_list.end()));
  }
  RemoveDuplicates(*asset_statements_with_includes_);
  asset_statements_with_includes_callbacks_.Notify(
      *asset_statements_with_includes_);
  DCHECK(asset_statements_with_includes_callbacks_.empty());
}

}  // namespace android_webview
