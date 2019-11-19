// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "net/third_party/uri_template/uri_template.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#endif

namespace chrome_browser_net {

bool IsValidDohTemplate(const std::string& server_template,
                        std::string* server_method) {
  std::string url_string;
  std::string test_query = "this_is_a_test_query";
  std::unordered_map<std::string, std::string> template_params(
      {{"dns", test_query}});
  std::set<std::string> vars_found;
  bool valid_template = uri_template::Expand(server_template, template_params,
                                             &url_string, &vars_found);
  if (!valid_template) {
    // The URI template is malformed.
    return false;
  }
  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIs("https")) {
    // The expanded template must be a valid HTTPS URL.
    return false;
  }
  if (url.host().find(test_query) != std::string::npos) {
    // The dns variable may not be part of the hostname.
    return false;
  }
  // If the template contains a dns variable, use GET, otherwise use POST.
  DCHECK(server_method);
  *server_method =
      (vars_found.find("dns") == vars_found.end()) ? "POST" : "GET";
  return true;
}

bool ShouldDisableDohForManaged() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (g_browser_process->browser_policy_connector()->HasMachineLevelPolicies())
    return true;
#endif
#if defined(OS_WIN)
  if (base::IsMachineExternallyManaged())
    return true;
#endif
  return false;
}
}  // namespace chrome_browser_net
