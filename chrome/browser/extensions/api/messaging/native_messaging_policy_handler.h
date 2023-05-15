// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_POLICY_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_POLICY_HANDLER_H_

#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace extensions {

// Implements additional checks for policies that are lists of Native Messaging
// Hosts.
class NativeMessagingHostListPolicyHandler : public policy::ListPolicyHandler {
 public:
  NativeMessagingHostListPolicyHandler(const char* policy_name,
                                       const char* pref_path,
                                       bool allow_wildcards);

  NativeMessagingHostListPolicyHandler(
      const NativeMessagingHostListPolicyHandler&) = delete;
  NativeMessagingHostListPolicyHandler& operator=(
      const NativeMessagingHostListPolicyHandler&) = delete;

  ~NativeMessagingHostListPolicyHandler() override;

 protected:
  // ListPolicyHandler methods:

  // Checks whether |value| contains a valid host name (or a wildcard).
  bool CheckListEntry(const base::Value& value) override;

  // Sets |prefs| at pref_path() to |filtered_list|.
  void ApplyList(base::Value::List filtered_list, PrefValueMap* prefs) override;

 private:
  const char* pref_path_;
  bool allow_wildcards_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_POLICY_HANDLER_H_
