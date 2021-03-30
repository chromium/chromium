// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kDevicesKey[] = "devices";
constexpr char kVendorIdKey[] = "vendor_id";
constexpr char kProductIdKey[] = "product_id";
constexpr char kUrlsKey[] = "urls";
constexpr char kErrorPathTemplate[] = "items[%d].%s.items[%d]";
constexpr char kMissingVendorIdError[] = "A vendor_id must also be specified";
constexpr char kInvalidNumberOfUrlsError[] =
    "Each urls string entry must contain between 1 to 2 URLs";
constexpr char kInvalidUrlError[] = "The urls item must contain valid URLs";

}  // namespace

WebUsbAllowDevicesForUrlsPolicyHandler::WebUsbAllowDevicesForUrlsPolicyHandler(
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kWebUsbAllowDevicesForUrls,
          chrome_schema.GetKnownProperty(key::kWebUsbAllowDevicesForUrls),
          SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN) {}

WebUsbAllowDevicesForUrlsPolicyHandler::
    ~WebUsbAllowDevicesForUrlsPolicyHandler() {}

bool WebUsbAllowDevicesForUrlsPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return true;

  bool result =
      SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors);

  std::string error_path;
  std::string error;
  if (!result)
    return result;

  int item_index = 0;
  for (const auto& item : value->GetList()) {
    // The vendor and product ID descriptors of a USB devices should be
    // unsigned short integers.
    int device_index = 0;
    auto* devices_list =
        item.FindKeyOfType(kDevicesKey, base::Value::Type::LIST);
    DCHECK(devices_list);
    for (const auto& device : devices_list->GetList()) {
      auto* vendor_id_value =
          device.FindKeyOfType(kVendorIdKey, base::Value::Type::INTEGER);
      auto* product_id_value =
          device.FindKeyOfType(kProductIdKey, base::Value::Type::INTEGER);
      if (product_id_value) {
        // If a |product_id| is specified, then a |vendor_id| must also be
        // specified. Otherwise, the policy is invalid.
        if (!vendor_id_value) {
          error_path = base::StringPrintf(kErrorPathTemplate, item_index,
                                          kDevicesKey, device_index);
          error = kMissingVendorIdError;
          result = false;
          break;
        }
      }
      ++device_index;
    }

    // The whitelisted URLs should be valid.
    int url_index = 0;
    auto* urls_list = item.FindKeyOfType(kUrlsKey, base::Value::Type::LIST);
    DCHECK(urls_list);
    for (const auto& url_value : urls_list->GetList()) {
      const std::string url_error_path = base::StringPrintf(
          kErrorPathTemplate, item_index, kUrlsKey, url_index);

      DCHECK(url_value.is_string());
      const std::vector<std::string> urls =
          base::SplitString(url_value.GetString(), ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL);
      if (urls.size() > 2 || urls.empty()) {
        error_path = url_error_path;
        error = kInvalidNumberOfUrlsError;
        result = false;
        break;
      }

      GURL requesting_url(urls[0]);
      if (!requesting_url.is_valid()) {
        error_path = url_error_path;
        error = kInvalidUrlError;
        result = false;
        break;
      }

      if (urls.size() == 2) {
        bool embedding_url_is_wildcard = urls[1].empty();
        GURL embedding_url(urls[1]);

        // Invalid URLs do not get stored in the GURL, so the string value is
        // checked to see if it is empty to signify a wildcard.
        if (!embedding_url_is_wildcard && !embedding_url.is_valid()) {
          error_path = url_error_path;
          error = kInvalidUrlError;
          result = false;
          break;
        }
      }

      ++url_index;
    }

    if (!error_path.empty() || !error.empty())
      break;

    ++item_index;
  }

  if (errors && !error.empty()) {
    if (error_path.empty())
      error_path = "(ROOT)";
    errors->AddError(policy_name(), error_path, error);
  }

  return result;
}

void WebUsbAllowDevicesForUrlsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> value;
  if (!CheckAndGetValue(policies, nullptr, &value))
    return;

  if (!value || !value->is_list())
    return;

  prefs->SetValue(prefs::kManagedWebUsbAllowDevicesForUrls,
                  base::Value::FromUniquePtrValue(std::move(value)));
}

}  // namespace policy
