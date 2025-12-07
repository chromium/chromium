// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/code_signature_mac.h"

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/crash/core/common/crash_key.h"

namespace {
// A crash key that is used when dumping because of errors when validating code
// signatures.
crash_reporter::CrashKeyString<256> code_signature_crash_key("CodeSignature");

// This function logs the status and error_details using OSSTATUS_LOG(). It also
// calls base::debug::DumpWithoutCrashing() using code_signature_crash_key
// as a crash key. The status and error_details are appended to the crash key.
void DumpOSStatusError(OSStatus status, std::string error_details) {
  OSSTATUS_LOG(ERROR, status) << error_details;
  crash_reporter::ScopedCrashKeyString crash_key_value(
      &code_signature_crash_key,
      base::StringPrintf("%s: %s (%d)", error_details.c_str(),
                         logging::DescriptionFromOSStatus(status).c_str(),
                         status));
  base::debug::DumpWithoutCrashing();
}

// This function is similar to DumpOSStatusError(), however it operates without
// an OSStatus.
void DumpError(std::string error_details) {
  LOG(ERROR) << error_details;
  crash_reporter::ScopedCrashKeyString crash_key_value(
      &code_signature_crash_key, error_details);
  base::debug::DumpWithoutCrashing();
}
}  // namespace

namespace apps {

base::expected<base::apple::ScopedCFTypeRef<CFStringRef>,
               MissingRequirementReason>
FrameworkBundleDesignatedRequirementString() {
  // Note: Don't validate |framework_code|: We don't need to waste time
  // validating. We are only interested in discovering if the framework bundle
  // is code-signed, and if so what the designated requirement is.
  base::apple::ScopedCFTypeRef<CFURLRef> framework_url =
      base::apple::FilePathToCFURL(base::apple::FrameworkBundlePath());
  base::apple::ScopedCFTypeRef<SecStaticCodeRef> framework_code;
  OSStatus status = SecStaticCodeCreateWithPath(
      framework_url.get(), kSecCSDefaultFlags, framework_code.InitializeInto());

  // If the framework bundle is unsigned there is nothing else to do. We treat
  // this as success because there’s no identity to protect or even match, so
  // it’s not dangerous to let the shim connect.
  if (status == errSecCSUnsigned) {
    return base::unexpected(MissingRequirementReason::NoOrAdHocSignature);
  }

  // If there was an error obtaining the SecStaticCodeRef something is very
  // broken or something bad is happening, deny.
  if (status != errSecSuccess) {
    DumpOSStatusError(status, "SecStaticCodeCreateWithPath");
    return base::unexpected(MissingRequirementReason::Error);
  }

  // Copy the signing info from the SecStaticCodeRef.
  base::apple::ScopedCFTypeRef<CFDictionaryRef> framework_signing_info;
  status = SecCodeCopySigningInformation(
      framework_code.get(), kSecCSSigningInformation,
      framework_signing_info.InitializeInto());
  if (status != errSecSuccess) {
    DumpOSStatusError(status, "SecCodeCopySigningInformation");
    return base::unexpected(MissingRequirementReason::Error);
  }

  // Look up the code signing flags. If the flags are absent treat this as
  // unsigned. This decision is consistent with the StaticCode source:
  // https://github.com/apple-oss-distributions/Security/blob/Security-60157.40.30.0.1/OSX/libsecurity_codesigning/lib/StaticCode.cpp#L2270
  CFNumberRef framework_signing_info_flags =
      base::apple::GetValueFromDictionary<CFNumberRef>(
          framework_signing_info.get(), kSecCodeInfoFlags);
  if (!framework_signing_info_flags) {
    return base::unexpected(MissingRequirementReason::NoOrAdHocSignature);
  }

  // If the framework bundle is ad-hoc signed there is nothing else to
  // do. While the framework bundle is code-signed an ad-hoc signature does not
  // contain any identities to match against. Treat this as a success.
  //
  // Note: Using a long long to extract the value from the CFNumberRef to be
  // consistent with how it was packed by Security.framework.
  // https://github.com/apple-oss-distributions/Security/blob/Security-60157.40.30.0.1/OSX/libsecurity_utilities/lib/cfutilities.h#L262
  long long flags;
  if (!CFNumberGetValue(framework_signing_info_flags, kCFNumberLongLongType,
                        &flags)) {
    DumpError("CFNumberGetValue");
    return base::unexpected(MissingRequirementReason::Error);
  }
  if (static_cast<uint32_t>(flags) & kSecCodeSignatureAdhoc) {
    return base::unexpected(MissingRequirementReason::NoOrAdHocSignature);
  }

  // Time to start building a requirement that we will use to validate
  // another code signature. First let's get the framework bundle requirement.
  // We will build a suitable requirement based off that.
  base::apple::ScopedCFTypeRef<SecRequirementRef> framework_requirement;
  status =
      SecCodeCopyDesignatedRequirement(framework_code.get(), kSecCSDefaultFlags,
                                       framework_requirement.InitializeInto());
  if (status != errSecSuccess) {
    DumpOSStatusError(status, "SecCodeCopyDesignatedRequirement");
    return base::unexpected(MissingRequirementReason::Error);
  }

  base::apple::ScopedCFTypeRef<CFStringRef> framework_requirement_string;
  status =
      SecRequirementCopyString(framework_requirement.get(), kSecCSDefaultFlags,
                               framework_requirement_string.InitializeInto());
  if (status != errSecSuccess) {
    DumpOSStatusError(status, "SecRequirementCopyString");
    DumpOSStatusError(status, "SecCodeCopyDesignatedRequirement");
  }

  return framework_requirement_string;
}

base::apple::ScopedCFTypeRef<SecRequirementRef> RequirementFromString(
    CFStringRef requirement_string) {
  base::apple::ScopedCFTypeRef<SecRequirementRef> requirement;
  OSStatus status = SecRequirementCreateWithString(
      requirement_string, kSecCSDefaultFlags, requirement.InitializeInto());
  if (status != errSecSuccess) {
    DumpOSStatusError(status,
                      std::string("SecRequirementCreateWithString: ") +
                          base::SysCFStringRefToUTF8(requirement_string));
    return base::apple::ScopedCFTypeRef<SecRequirementRef>(nullptr);
  }

  return requirement;
}

}  // namespace apps
