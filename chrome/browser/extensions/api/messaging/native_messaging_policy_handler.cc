// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_messaging_policy_handler.h"
#include "components/prefs/pref_value_map.h"

#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"

namespace extensions {

NativeMessagingHostListPolicyHandler::NativeMessagingHostListPolicyHandler(
    const char* policy_name,
    const char* pref_path,
    bool allow_wildcards)
    : policy::ListPolicyHandler(policy_name, base::Value::Type::STRING),
      pref_path_(pref_path),
      allow_wildcards_(allow_wildcards) {}

NativeMessagingHostListPolicyHandler::~NativeMessagingHostListPolicyHandler() {}

bool NativeMessagingHostListPolicyHandler::CheckListEntry(
    const base::Value& value) {
  const std::string& str = value.GetString();
  if (allow_wildcards_ && str == "*")
    return true;

  return NativeMessagingHostManifest::IsValidName(str);
}

void NativeMessagingHostListPolicyHandler::ApplyList(base::Value filtered_list,
                                                     PrefValueMap* prefs) {
  DCHECK(filtered_list.is_list());
  prefs->SetValue(pref_path_, std::move(filtered_list));
}

}  // namespace extensions
