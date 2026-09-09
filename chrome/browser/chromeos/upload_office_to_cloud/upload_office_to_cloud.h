// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_
#define CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_

class Profile;
class PrefRegistrySimple;

namespace chromeos {

// Return True if user of the `profile` is eligible for Office files upload.
// A user is eligible if:
// - They are not in Guest mode.
// - They are not a child profile.
bool IsEligibleAndEnabledUploadOfficeToCloud(const Profile* profile);

namespace cloud_upload {

inline constexpr char kCloudUploadPolicyAllowed[] = "allowed";
inline constexpr char kCloudUploadPolicyDisallowed[] = "disallowed";
inline constexpr char kCloudUploadPolicyAutomated[] = "automated";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if the MicrosoftOneDriveMount policy is set to `allowed` or
// `automated` and false otherwise.
bool IsMicrosoftOfficeOneDriveIntegrationAllowed(const Profile* profile);

// Returns true if the MicrosoftOneDriveMount policy is set to `automated` and
// false otherwise.
bool IsMicrosoftOfficeOneDriveIntegrationAutomated(const Profile* profile);

// Returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `ash::prefs::kMicrosoftOfficeCloudUpload` is set to `allowed` or
// `automated`.
bool IsMicrosoftOfficeCloudUploadAllowed(Profile* profile);

// Returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `ash::prefs::kMicrosoftOfficeCloudUpload` is set to `automated`.
bool IsMicrosoftOfficeCloudUploadAutomated(Profile* profile);

// Returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `ash::prefs::kGoogleWorkspaceCloudUpload` is set to `allowed` or
// `automated`.
bool IsGoogleWorkspaceCloudUploadAllowed(Profile* profile);

// Returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `ash::prefs::kGoogleWorkspaceCloudUpload` is set to `automated`.
bool IsGoogleWorkspaceCloudUploadAutomated(Profile* profile);

}  // namespace cloud_upload

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_
