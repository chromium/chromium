// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_
#define CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_

class Profile;
class PrefRegistrySimple;

namespace chromeos {

// Return True if feature `kUploadOfficeToCloud` is enabled and is eligible for
// the user of the `profile`.  A user is eligible if:
// - They are not in Guest mode.
// - They are not managed.
// - They are not a child profile and `kUploadOfficeToCloudForEnterprise` is
// enabled.
bool IsEligibleAndEnabledUploadOfficeToCloud(const Profile* profile);

namespace cloud_upload {

constexpr char kCloudUploadPolicyAllowed[] = "allowed";
constexpr char kCloudUploadPolicyDisallowed[] = "disallowed";
constexpr char kCloudUploadPolicyAutomated[] = "automated";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns true if the MicrosoftOneDriveMount policy is set to `allowed` or
// `automated` and false otherwise.
bool IsMicrosoftOfficeOneDriveIntegrationAllowed(const Profile* profile);

// If `kUploadOfficeToCloudForEnterprise` is disabled, returns true if
// IsEligibleAndEnabledUploadOfficeToCloud() is true.
// Otherwise returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `prefs::kMicrosoftOfficeCloudUpload` is set to `allowed` or `automated`.
bool IsMicrosoftOfficeCloudUploadAllowed(Profile* profile);

// If `kUploadOfficeToCloudForEnterprise` is disabled, returns false.
// Otherwise returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `prefs::kMicrosoftOfficeCloudUpload` is set to `automated`.
bool IsMicrosoftOfficeCloudUploadAutomated(Profile* profile);

// If `kUploadOfficeToCloudForEnterprise` is disabled, returns true if
// IsEligibleAndEnabledUploadOfficeToCloud() is true.
// Otherwise returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `prefs::kGoogleWorkspaceCloudUpload` is set to `allowed` or `automated`.
bool IsGoogleWorkspaceCloudUploadAllowed(Profile* profile);

// If `kUploadOfficeToCloudForEnterprise` is disabled, returns false.
// Otherwise returns true if IsEligibleAndEnabledUploadOfficeToCloud() is true
// and `prefs::kGoogleWorkspaceCloudUpload` is set to `automated`.
bool IsGoogleWorkspaceCloudUploadAutomated(Profile* profile);

}  // namespace cloud_upload

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_
