// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/external_cache.h"

#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/common/extension_urls.h"

namespace chromeos {

// static
GURL ExternalCache::GetExtensionUpdateUrl(
    const base::Value::Dict& extension_value,
    bool always_checking_for_updates) {
  const auto keep_if_present_opt = extension_value.FindBool(
      extensions::ExternalProviderImpl::kKeepIfPresent);

  // "keep if present" means that the extension should be kept if it's already
  // present, but it should not be added to extensions system.
  if (keep_if_present_opt.value_or(false))
    return GURL();

  const std::string* external_update_url = extension_value.FindString(
      extensions::ExternalProviderImpl::kExternalUpdateUrl);

  if (external_update_url && !external_update_url->empty()) {
    return GURL(*external_update_url);
  }

  if (always_checking_for_updates)
    return extension_urls::GetWebstoreUpdateUrl();

  return GURL();
}

// static
base::Value::Dict ExternalCache::GetExtensionValueToCache(
    const base::Value::Dict& extension,
    const std::string& path,
    const std::string& version) {
  base::Value::Dict result = extension.Clone();

  const std::string* external_update_url_value = extension.FindString(
      extensions::ExternalProviderImpl::kExternalUpdateUrl);
  if (external_update_url_value &&
      extension_urls::IsWebstoreUpdateUrl(GURL(*external_update_url_value))) {
    result.Set(extensions::ExternalProviderImpl::kIsFromWebstore, true);
  }
  result.Remove(extensions::ExternalProviderImpl::kExternalUpdateUrl);

  result.Set(extensions::ExternalProviderImpl::kExternalVersion, version);
  result.Set(extensions::ExternalProviderImpl::kExternalCrx, path);
  return result;
}

// static
bool ExternalCache::ShouldCacheImmediately(const base::Value::Dict& extension) {
  const auto keep_if_present_opt =
      extension.FindBool(extensions::ExternalProviderImpl::kKeepIfPresent);

  return (keep_if_present_opt.value_or(false)) ||
         extension.Find(extensions::ExternalProviderImpl::kExternalCrx);
}

}  // namespace chromeos
