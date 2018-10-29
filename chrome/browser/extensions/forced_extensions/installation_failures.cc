// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/installation_failures.h"

#include <map>

#include "base/logging.h"
#include "base/no_destructor.h"

namespace {

using FailureMap =
    std::map<extensions::ExtensionId, extensions::InstallationFailures::Reason>;

FailureMap& GetInstallationFailureMap(const Profile* profile) {
  static base::NoDestructor<std::map<const Profile*, FailureMap>> failure_maps;
  return (*failure_maps)[profile];
}

}  // namespace

namespace extensions {

// static
void InstallationFailures::ReportFailure(const Profile* profile,
                                         const ExtensionId& id,
                                         Reason reason) {
  DCHECK_NE(reason, Reason::UNKNOWN);
  GetInstallationFailureMap(profile).emplace(id, reason);
}

// static
InstallationFailures::Reason InstallationFailures::Get(const Profile* profile,
                                                       const ExtensionId& id) {
  FailureMap& map = GetInstallationFailureMap(profile);
  auto it = map.find(id);
  return it == map.end() ? Reason::UNKNOWN : it->second;
}

// static
void InstallationFailures::Clear(const Profile* profile) {
  GetInstallationFailureMap(profile).clear();
}

}  //  namespace extensions
