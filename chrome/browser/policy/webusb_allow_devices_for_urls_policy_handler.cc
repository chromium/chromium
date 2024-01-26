// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kDevicesKey[] = "devices";
constexpr char kVendorIdKey[] = "vendor_id";
constexpr char kProductIdKey[] = "product_id";
constexpr char kUrlsKey[] = "urls";

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
  if (!policies.IsPolicySet(policy_name()))
    return true;
  bool result =
      SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors);

  PolicyErrorPath error_path;
  int error_message_id;
  if (!result)
    return result;

  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  DCHECK(value);
  int item_index = 0;
  for (const auto& item : value->GetList()) {
    // The vendor and product ID descriptors of a USB devices should be
    // unsigned short integers.
    int device_index = 0;
    auto* devices_list = item.GetDict().FindList(kDevicesKey);
    DCHECK(devices_list);
    for (const auto& device : *devices_list) {
      std::optional<int> vendor_id = device.GetDict().FindInt(kVendorIdKey);
      std::optional<int> product_id = device.GetDict().FindInt(kProductIdKey);
      if (product_id.has_value()) {
        // If a |product_id| is specified, then a |vendor_id| must also be
        // specified. Otherwise, the policy is invalid.
        if (!vendor_id.has_value()) {
          error_path = {item_index, kDevicesKey, device_index};
          error_message_id = IDS_POLICY_MISSING_VENDOR_ID_ERROR;
          result = false;
          break;
        }
      }
      ++device_index;
    }

    // The allowlisted URLs should be valid.
    int url_index = 0;
    auto* urls_list = item.GetDict().FindList(kUrlsKey);
    DCHECK(urls_list);
    for (const auto& url_value : *urls_list) {
      PolicyErrorPath url_error_path = {item_index, kUrlsKey, url_index};

      DCHECK(url_value.is_string());
      const std::vector<std::string> urls =
          base::SplitString(url_value.GetString(), ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL);
      if (urls.size() > 2 || urls.empty()) {
        error_path = url_error_path;
        error_message_id = IDS_POLICY_INVALID_NUMBER_OF_URLS_ERROR;
        result = false;
        break;
      }

      GURL requesting_url(urls[0]);
      if (!requesting_url.is_valid()) {
        error_path = url_error_path;
        error_message_id = IDS_POLICY_INVALID_URL_ERROR;
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
          error_message_id = IDS_POLICY_INVALID_URL_ERROR;
          result = false;
          break;
        }
      }

      ++url_index;
    }

    if (!result)
      break;

    ++item_index;
  }

  if (errors && !result) {
    errors->AddError(policy_name(), error_message_id, error_path);
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
