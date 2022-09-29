// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_cache.h"

#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "extensions/common/extension_urls.h"

namespace chromeos {

// static
GURL ExternalCache::GetExtensionUpdateUrl(const base::Value& extension_value,
                                          bool always_checking_for_updates) {
  DCHECK(extension_value.is_dict());

  const base::Value* keep_if_present_value = extension_value.FindKeyOfType(
      extensions::ExternalProviderImpl::kKeepIfPresent,
      base::Value::Type::BOOLEAN);

  // "keep if present" means that the extension should be kept if it's already
  // present, but it should not be added to extesions system.
  if (keep_if_present_value && keep_if_present_value->GetBool())
    return GURL();

  const base::Value* external_update_url_value = extension_value.FindKeyOfType(
      extensions::ExternalProviderImpl::kExternalUpdateUrl,
      base::Value::Type::STRING);

  if (external_update_url_value &&
      !external_update_url_value->GetString().empty()) {
    return GURL(external_update_url_value->GetString());
  }

  if (always_checking_for_updates)
    return extension_urls::GetWebstoreUpdateUrl();

  return GURL();
}

void ExternalCache::UpdateExtensionsListWithDict(base::Value::Dict prefs) {
  UpdateExtensionsList(base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(base::Value(std::move(prefs)))));
}

// static
base::Value ExternalCache::GetExtensionValueToCache(
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
  return base::Value(std::move(result));
}

// static
bool ExternalCache::ShouldCacheImmediately(const base::Value& extension) {
  DCHECK(extension.is_dict());

  const base::Value* keep_if_present_value =
      extension.FindKeyOfType(extensions::ExternalProviderImpl::kKeepIfPresent,
                              base::Value::Type::BOOLEAN);

  return (keep_if_present_value && keep_if_present_value->GetBool()) ||
         extension.FindKey(extensions::ExternalProviderImpl::kExternalCrx);
}

}  // namespace chromeos
