// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_
#define CHROME_BROWSER_CHROMEOS_UPLOAD_OFFICE_TO_CLOUD_UPLOAD_OFFICE_TO_CLOUD_H_

class Profile;
class PrefRegistrySimple;

namespace chromeos {

// Return True if feature `kUploadOfficeToCloud` is enabled and is eligible for
// the user of the `profile`. A user is eligible if they are not managed.
// If `kUploadOfficeToCloudForEnterprise` is enabled too, the condition is
// loosened and the user becomes eligible if they're not a child profile.
bool IsEligibleAndEnabledUploadOfficeToCloud(Profile* profile);

namespace cloud_upload {

constexpr char kCloudUploadPolicyAllowed[] = "allowed";
constexpr char kCloudUploadPolicyDisallowed[] = "disallowed";
constexpr char kCloudUploadPolicyAutomated[] = "automated";

void RegisterProfilePrefs(PrefRegistrySimple* registry);

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
