// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_policy_migrator.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "extensions/common/hashed_extension_id.h"

namespace browser_switcher {

namespace {

void SecondsToMilliseconds(base::Value* val) {
  const int ms_per_second = 1000;
  *val = base::Value(val->GetInt() * ms_per_second);
}

// Transforms the string policy to a list policy (containing 1 string).
//
// The LBS extension's command-line parameter policies are single strings,
// because on Windows the command-line parameters are passed as a single string
// to the program. The parameters are parsed by the program, not the shell.
//
// On other platforms though, parameter parsing is done by the shell, not the
// program. So the new policies are string-lists that are given to the program
// pre-parsed. This is why we need to convert the string to a list, when
// migrating from the old policy.
void StringToList(base::Value* val) {
  std::string str = val->GetString();
  *val = base::Value(base::Value::Type::LIST);
  val->Append(base::Value(std::move(str)));
}

}  // namespace

const char kLBSExtensionId[] = "heildphpnddilhkemkielfhnkaagiabh";

BrowserSwitcherPolicyMigrator::BrowserSwitcherPolicyMigrator() = default;
BrowserSwitcherPolicyMigrator::~BrowserSwitcherPolicyMigrator() = default;

void BrowserSwitcherPolicyMigrator::Migrate(policy::PolicyBundle* bundle) {
  policy::PolicyMap& extension_map = bundle->Get(policy::PolicyNamespace(
      policy::POLICY_DOMAIN_EXTENSIONS, kLBSExtensionId));
  policy::PolicyMap& chrome_map =
      bundle->Get(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, ""));
  if (extension_map.empty())
    return;

  const auto* entry = chrome_map.Get("BrowserSwitcherEnabled");
  if (!entry || !entry->value || !entry->value->GetBool())
    return;
  extension_map.Set("browser_switcher_enabled", entry->DeepCopy());

  using Migration = policy::ExtensionPolicyMigrator::Migration;
  const Migration migrations[] = {
      Migration("alternative_browser_path",
                policy::key::kAlternativeBrowserPath),
      Migration("chrome_path", policy::key::kBrowserSwitcherChromePath),
      Migration("url_list", policy::key::kBrowserSwitcherUrlList),
      Migration("url_greylist", policy::key::kBrowserSwitcherUrlGreylist),
      Migration("keep_last_chrome_tab",
                policy::key::kBrowserSwitcherKeepLastChromeTab),
      Migration("use_ie_site_list", policy::key::kBrowserSwitcherUseIeSitelist),
      Migration("show_transition_screen", policy::key::kBrowserSwitcherDelay,
                base::BindRepeating(&SecondsToMilliseconds)),
      Migration("chrome_arguments",
                policy::key::kBrowserSwitcherChromeParameters,
                base::BindRepeating(&StringToList)),
      Migration("alternative_browser_arguments",
                policy::key::kAlternativeBrowserParameters,
                base::BindRepeating(&StringToList)),
  };

  CopyPoliciesIfUnset(bundle,
                      extensions::HashedExtensionId(kLBSExtensionId).value(),
                      migrations);
}

}  // namespace browser_switcher
