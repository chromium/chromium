// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_util.h"

#include "third_party/re2/src/re2/re2.h"

namespace borealis {

const char kBorealisAppId[] = "dkecggknbdokeipkgnhifhiokailichf";
const char kBorealisMainAppId[] = "epfhbkiklgmlkhfpbcdleadnhcfdjfmo";
const char kBorealisDlcName[] = "borealis-dlc";
// TODO(b/174282035): Potentially update regex when other strings
// are updated.
const char kBorealisAppIdRegex[] = "([^/]+\\d+)";

absl::optional<int> GetBorealisAppId(std::string exec) {
  int app_id;
  if (RE2::PartialMatch(exec, kBorealisAppIdRegex, &app_id)) {
    return app_id;
  } else {
    return absl::nullopt;
  }
}

}  // namespace borealis
