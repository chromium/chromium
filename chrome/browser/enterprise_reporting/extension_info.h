// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_INFO_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_INFO_H_

#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

class Profile;

namespace enterprise_reporting {

void AppendExtensionInfoIntoProfileReport(
    Profile* profile,
    em::ChromeUserProfileInfo* profile_info);

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_INFO_H_
