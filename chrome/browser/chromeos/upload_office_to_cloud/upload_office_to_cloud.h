// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_
#define CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_

class Profile;

namespace chromeos {

// Return True if feature `kUploadOfficeToCloud` is enabled and is eligible for
// the user of the `profile`. A user is eligible if they are not managed.
bool IsEligibleAndEnabledUploadOfficeToCloud(Profile* profile);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_
