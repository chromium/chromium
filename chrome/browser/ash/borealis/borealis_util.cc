// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "third_party/re2/src/re2/re2.h"

namespace borealis {

const char kBorealisAppId[] = "dkecggknbdokeipkgnhifhiokailichf";
const char kBorealisDlcName[] = "borealis-dlc";
// TODO(b/174282035): Potentially update regex when other strings
// are updated.
const char kBorealisAppIdRegex[] = "([^/]+\\d+)";

bool GetBorealisAppId(std::string exec, int& app_id) {
  return RE2::PartialMatch(exec, kBorealisAppIdRegex, &app_id);
}

}  // namespace borealis
