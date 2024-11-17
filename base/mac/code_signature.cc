// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/code_signature.h"

#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/info_plist_data.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

using base::apple::ScopedCFTypeRef;

namespace base::mac {

namespace {

// Return a dictionary of attributes suitable for looking up `process` with
// `SecCodeCopyGuestWithAttributes`.
ScopedCFTypeRef<CFDictionaryRef> AttributesForGuestValidation(
    absl::variant<audit_token_t, pid_t> process,
    SignatureValidationType validation_type,
    std::string_view info_plist_xml) {
  ScopedCFTypeRef<CFMutableDictionaryRef> attributes(
      CFDictionaryCreateMutable(nullptr, 3, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));

  if (audit_token_t* token = absl::get_if<audit_token_t>(&process)) {
    ScopedCFTypeRef<CFDataRef> audit_token_cf(CFDataCreate(
        nullptr, reinterpret_cast<const UInt8*>(token), sizeof(audit_token_t)));
    CFDictionarySetValue(attributes.get(), kSecGuestAttributeAudit,
                         audit_token_cf.get());
  } else {
    CHECK(absl::holds_alternative<pid_t>(process));
    ScopedCFTypeRef<CFNumberRef> pid_cf(
        CFNumberCreate(nullptr, kCFNumberIntType, &absl::get<pid_t>(process)));
    CFDictionarySetValue(attributes.get(), kSecGuestAttributePid, pid_cf.get());
  }

  if (validation_type == SignatureValidationType::DynamicOnly) {
    ScopedCFTypeRef<CFDataRef> info_plist(CFDataCreate(
        nullptr, reinterpret_cast<const UInt8*>(info_plist_xml.data()),
        static_cast<CFIndex>(info_plist_xml.length())));

    CFDictionarySetValue(attributes.get(), kSecGuestAttributeDynamicCode,
                         kCFBooleanTrue);
    CFDictionarySetValue(attributes.get(),
                         kSecGuestAttributeDynamicCodeInfoPlist,
                         info_plist.get());
  }

  return attributes;
}

OSStatus ValidateGuestWithAttributes(CFDictionaryRef attributes,
                                     SecRequirementRef requirement) {
  ScopedCFTypeRef<SecCodeRef> code;
  OSStatus status = SecCodeCopyGuestWithAttributes(
      nullptr, attributes, kSecCSDefaultFlags, code.InitializeInto());
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status) << "SecCodeCopyGuestWithAttributes";
    return status;
  }
  return SecCodeCheckValidity(code.get(), kSecCSDefaultFlags, requirement);
}

}  // namespace

OSStatus ProcessIsSignedAndFulfillsRequirement(
    audit_token_t audit_token,
    SecRequirementRef requirement,
    SignatureValidationType validation_type,
    std::string_view info_plist_xml) {
  ScopedCFTypeRef<CFDictionaryRef> attributes = AttributesForGuestValidation(
      audit_token, validation_type, info_plist_xml);
  return ValidateGuestWithAttributes(attributes.get(), requirement);
}

OSStatus ProcessIdIsSignedAndFulfillsRequirement_DoNotUse(
    pid_t pid,
    SecRequirementRef requirement,
    SignatureValidationType validation_type,
    std::string_view info_plist_xml) {
  ScopedCFTypeRef<CFDictionaryRef> attributes =
      AttributesForGuestValidation(pid, validation_type, info_plist_xml);
  return ValidateGuestWithAttributes(attributes.get(), requirement);
}

ScopedCFTypeRef<SecRequirementRef> RequirementFromString(
    std::string_view requirement_string) {
  ScopedCFTypeRef<CFStringRef> requirement_string_cf(
      base::SysUTF8ToCFStringRef(requirement_string));
  ScopedCFTypeRef<SecRequirementRef> requirement;
  OSStatus status = SecRequirementCreateWithString(
      requirement_string_cf.get(), kSecCSDefaultFlags,
      requirement.InitializeInto());
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status)
        << "SecRequirementCreateWithString: " << requirement_string;
    return base::apple::ScopedCFTypeRef<SecRequirementRef>(nullptr);
  }

  return requirement;
}

base::expected<ScopedCFTypeRef<SecCodeRef>, OSStatus>
DynamicCodeObjectForCurrentProcess() {
  std::vector<uint8_t> info_plist_xml = OuterBundleCachedInfoPlistData();
  ScopedCFTypeRef<CFDictionaryRef> attributes = AttributesForGuestValidation(
      getpid(), SignatureValidationType::DynamicOnly,
      base::as_string_view(info_plist_xml));

  ScopedCFTypeRef<SecCodeRef> code;
  OSStatus status = SecCodeCopyGuestWithAttributes(
      nullptr, attributes.get(), kSecCSDefaultFlags, code.InitializeInto());
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status) << "SecCodeCopyGuestWithAttributes";
    return base::unexpected(status);
  }

  return code;
}

}  // namespace base::mac
