// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_POLICY_MANAGED_CONFIGURATION_VARIABLES_H_
#define CHROME_BROWSER_ASH_ARC_POLICY_MANAGED_CONFIGURATION_VARIABLES_H_

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"

namespace arc {

extern const char kUserEmail[];
extern const char kUserEmailName[];
extern const char kUserEmailDomain[];
extern const char kDeviceDirectoryId[];
extern const char kDeviceSerialNumber[];
extern const char kDeviceAssetId[];
extern const char kDeviceAnnotatedLocation[];

// Replace the supported "template variables" for the managed configuration of
// Android apps in ARC++ from the given |managedConfiguration|.
//
// Supported template variables are:
// * ${USER_EMAIL} - Email address of the primary signed in user.
// * ${USER_EMAIL_NAME} - Part before "@" of the user email address.
// * ${USER_EMAIL_DOMAIN} - Domain name of the user email address.
// * ${DEVICE_DIRECTORY_ID} - Device directory ID.
// * ${DEVICE_SERIAL_NUMBER} - Device serial number.
// * ${DEVICE_ASSET_ID} - Asset ID assigned by administrator.
// * ${DEVICE_ANNOTATED_LOCATION} - Location assigned by administrator.
void RecursivelyReplaceManagedConfigurationVariables(
    const Profile* profile,
    base::Value* managedConfiguration);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_MANAGED_CONFIGURATION_VARIABLES_H_
