// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_SERIALIZE_HOST_DESCRIPTIONS_H_
#define CHROME_BROWSER_DEVTOOLS_SERIALIZE_HOST_DESCRIPTIONS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"

// DevToolsAgentHost description to be serialized by SerializeHostDescriptions.
struct HostDescriptionNode {
  std::string name;
  std::string parent_name;
  base::Value representation;
};

// A helper function taking a HostDescriptionNode representation of hosts and
// producing a list of representations. The representation contains a list of
// dictionaries for each root in host, and has dictionaries of children
// injected into a list keyed |child_key| in the parent's dictionary.
base::Value::List SerializeHostDescriptions(
    std::vector<HostDescriptionNode> hosts,
    std::string_view child_key);

#endif  // CHROME_BROWSER_DEVTOOLS_SERIALIZE_HOST_DESCRIPTIONS_H_
