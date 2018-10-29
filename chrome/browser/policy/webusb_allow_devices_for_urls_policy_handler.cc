// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/webusb_allow_devices_for_urls_policy_handler.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

namespace {

constexpr char kDevicesKey[] = "devices";
constexpr char kVendorIdKey[] = "vendor_id";
constexpr char kProductIdKey[] = "product_id";
constexpr char kErrorPathTemplate[] = "items[%d].devices.items[%d]";
constexpr char kInvalidUnsignedShortIntErrorTemplate[] =
    "The %s must be an unsigned short integer";
constexpr char kMissingVendorIdError[] = "A vendor_id must also be specified";

}  // namespace

WebUsbAllowDevicesForUrlsPolicyHandler::WebUsbAllowDevicesForUrlsPolicyHandler(
    Schema schema)
    : SchemaValidatingPolicyHandler(
          key::kWebUsbAllowDevicesForUrls,
          schema.GetKnownProperty(key::kWebUsbAllowDevicesForUrls),
          SchemaOnErrorStrategy::SCHEMA_STRICT) {}

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
  if (result) {
    // The vendor and product ID descriptors of a USB devices should be
    // unsigned short integers.
    int item_index = 0;
    int device_index = 0;
    for (const auto& item : value->GetList()) {
      DCHECK(item.FindKey(kDevicesKey));

      for (const auto& device : item.FindKey(kDevicesKey)->GetList()) {
        if (device.FindKey(kVendorIdKey)) {
          DCHECK(device.FindKey(kVendorIdKey)->is_int());

          const int vendor_id = device.FindKey(kVendorIdKey)->GetInt();
          if (vendor_id > 0xFFFF || vendor_id < 0) {
            error_path = base::StringPrintf(kErrorPathTemplate, item_index,
                                            device_index);
            error = base::StringPrintf(kInvalidUnsignedShortIntErrorTemplate,
                                       kVendorIdKey);
            result = false;
            break;
          }
        }

        if (device.FindKey(kProductIdKey)) {
          // If a |product_id| is specified, then a |vendor_id| must also be
          // specified. Otherwise, the device policy is invalid.
          if (device.FindKey(kVendorIdKey)) {
            DCHECK(device.FindKey(kProductIdKey)->is_int());

            const int product_id = device.FindKey(kProductIdKey)->GetInt();
            if (product_id > 0xFFFF || product_id < 0) {
              error_path = base::StringPrintf(kErrorPathTemplate, item_index,
                                              device_index);
              error = base::StringPrintf(kInvalidUnsignedShortIntErrorTemplate,
                                         kProductIdKey);
              result = false;
              break;
            }
          } else {
            error_path = base::StringPrintf(kErrorPathTemplate, item_index,
                                            device_index);
            error = kMissingVendorIdError;
            result = false;
            break;
          }
        }
        ++device_index;
      }

      if (!error_path.empty() || error.empty())
        break;

      ++item_index;
    }

    if (errors && !error.empty()) {
      if (error_path.empty())
        error_path = "(ROOT)";
      errors->AddError(policy_name(), error_path, error);
    }
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

  prefs->SetValue(prefs::kManagedWebUsbAllowDevicesForUrls, std::move(value));
}

}  // namespace policy
