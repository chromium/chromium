// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_mac.h"

#include <string>

#include "base/base64url.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment/");
const CFStringRef kEnrollmentTokenPolicyName =
    CFSTR("CloudManagementEnrollmentToken");
const char kEnrollmentTokenFilePath[] =
    FILE_PATH_LITERAL("/Library/Google/Chrome/CloudManagementEnrollmentToken");

// Enrollment Mandatory Option
const CFStringRef kEnrollmentMandatoryOptionPolicyName =
    CFSTR("CloudManagementEnrollmentMandatory");
const char kEnrollmentOptionsFilePath[] = FILE_PATH_LITERAL(
    "/Library/Google/Chrome/CloudManagementEnrollmentOptions");
const char kEnrollmentMandatoryOption[] = "Mandatory";

// Explicitly access the "com.google.Chrome" bundle ID, no matter what this
// app's bundle ID actually is. All channels of Chrome should obey the same
// policies.
const CFStringRef kBundleId = CFSTR("com.google.Chrome");

constexpr char kEnrollmentTokenMetricsName[] =
    "Enterprise.CloudManagementEnrollmentTokenLocation.Mac";

enum EnrollmentTokenLocation {
  kPolicy = 0,
  kFile = 1,
  kMaxValue = kFile,
};

bool GetDmTokenFilePath(base::FilePath* token_file_path,
                        const std::string& client_id,
                        bool create_dir) {
  if (!base::PathService::Get(base::DIR_APP_DATA, token_file_path))
    return false;

  *token_file_path = token_file_path->Append(kDmTokenBaseDir);

  if (create_dir && !base::CreateDirectory(*token_file_path))
    return false;

  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(client_id),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  *token_file_path = token_file_path->Append(filename.c_str());

  return true;
}

bool StoreDMTokenInDirAppDataDir(const std::string& token,
                                 const std::string& client_id) {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, client_id, /*create_dir=*/true)) {
    NOTREACHED();
    return false;
  }

  return base::ImportantFileWriter::WriteFileAtomically(token_file_path, token);
}

bool DeleteDMTokenFromAppDataDir(const std::string& client_id) {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, client_id, /*create_dir=*/false)) {
    NOTREACHED();
    return false;
  }

  return base::DeleteFile(token_file_path);
}

// Get the enrollment token from policy file: /Library/com.google.Chrome.plist.
// Return true if policy is set, otherwise false.
bool GetEnrollmentTokenFromPolicy(std::string* enrollment_token) {
  // Since the configuration management infrastructure is not initialized when
  // this code runs, read the policy preference directly.
  base::ScopedCFTypeRef<CFPropertyListRef> value(
      CFPreferencesCopyAppValue(kEnrollmentTokenPolicyName, kBundleId));

  // Read the enrollment token from the new location. If that fails, try the old
  // location (which will be deprecated soon). If that also fails, bail as there
  // is no token set.
  if (!value ||
      !CFPreferencesAppValueIsForced(kEnrollmentTokenPolicyName, kBundleId)) {
    return false;
  }
  CFStringRef value_string = base::mac::CFCast<CFStringRef>(value);
  if (!value_string)
    return false;

  *enrollment_token = base::SysCFStringRefToUTF8(value_string);
  return true;
}

bool GetEnrollmentTokenFromFile(std::string* enrollment_token) {
  // Read the enrollment token from the new location. If that fails, try the old
  // location (which will be deprecated soon). If that also fails, bail as there
  // is no token set.
  if (!base::ReadFileToString(base::FilePath(kEnrollmentTokenFilePath),
                              enrollment_token)) {
    return false;
  }
  *enrollment_token =
      std::string(base::TrimWhitespaceASCII(*enrollment_token, base::TRIM_ALL));
  return true;
}

absl::optional<bool> IsEnrollmentMandatoryByPolicy() {
  base::ScopedCFTypeRef<CFPropertyListRef> value(CFPreferencesCopyAppValue(
      kEnrollmentMandatoryOptionPolicyName, kBundleId));

  if (!value || !CFPreferencesAppValueIsForced(
                    kEnrollmentMandatoryOptionPolicyName, kBundleId)) {
    return absl::optional<bool>();
  }

  CFBooleanRef value_bool = base::mac::CFCast<CFBooleanRef>(value);
  if (!value_bool)
    return absl::optional<bool>();
  return value_bool == kCFBooleanTrue;
}

absl::optional<bool> IsEnrollmentMandatoryByFile() {
  std::string options;
  if (!base::ReadFileToString(base::FilePath(kEnrollmentOptionsFilePath),
                              &options)) {
    return absl::optional<bool>();
  }
  return std::string(base::TrimWhitespaceASCII(options, base::TRIM_ALL)) ==
         kEnrollmentMandatoryOption;
}

}  // namespace

BrowserDMTokenStorageMac::BrowserDMTokenStorageMac()
    : task_runner_(base::ThreadPool::CreateTaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageMac::~BrowserDMTokenStorageMac() {}

std::string BrowserDMTokenStorageMac::InitClientId() {
  if (client_id_.empty()) {
    client_id_ = base::mac::GetPlatformSerialNumber();
  }

  return client_id_;
}

std::string BrowserDMTokenStorageMac::InitEnrollmentToken() {
  std::string enrollment_token;
  if (GetEnrollmentTokenFromPolicy(&enrollment_token)) {
    base::UmaHistogramEnumeration(kEnrollmentTokenMetricsName,
                                  EnrollmentTokenLocation::kPolicy);
    return enrollment_token;
  }

  if (GetEnrollmentTokenFromFile(&enrollment_token)) {
    base::UmaHistogramEnumeration(kEnrollmentTokenMetricsName,
                                  EnrollmentTokenLocation::kFile);
    return enrollment_token;
  }

  return std::string();
}

std::string BrowserDMTokenStorageMac::InitDMToken() {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, InitClientId(),
                          /*create_dir=*/false))
    return std::string();

  std::string token;
  if (!base::ReadFileToString(token_file_path, &token))
    return std::string();

  return std::string(base::TrimWhitespaceASCII(token, base::TRIM_ALL));
}

bool BrowserDMTokenStorageMac::InitEnrollmentErrorOption() {
  absl::optional<bool> is_mandatory = IsEnrollmentMandatoryByPolicy();
  if (is_mandatory)
    return is_mandatory.value();

  return IsEnrollmentMandatoryByFile().value_or(false);
}

bool BrowserDMTokenStorageMac::CanInitEnrollmentToken() const {
  return true;
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageMac::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce(&StoreDMTokenInDirAppDataDir, token, client_id);
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageMac::DeleteDMTokenTask(
    const std::string& client_id) {
  return base::BindOnce(&DeleteDMTokenFromAppDataDir, client_id);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageMac::SaveDMTokenTaskRunner() {
  return task_runner_;
}

}  // namespace policy
