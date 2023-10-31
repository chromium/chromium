// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_evaluator_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/xattr.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/mac_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// macOS code signing data can be stored in extended attributes as well. This is
// a list of the extended attributes slots currently used in Security.framework,
// from codesign.h (see the kSecCS_* constants).
const char* const xattrs[] = {
      "com.apple.cs.CodeDirectory",
      "com.apple.cs.CodeSignature",
      "com.apple.cs.CodeRequirements",
      "com.apple.cs.CodeResources",
      "com.apple.cs.CodeApplication",
      "com.apple.cs.CodeEntitlements",
};

// The name of the localization strings file.
const char kStringsFile[] = ".lproj/InfoPlist.strings";

// Convenience function to get the appropriate path from a variety of Core
// Foundation types. For resources, code signing seems to give back a URL in
// which the path is relative to the bundle root. So in this case, we take the
// relative component, otherwise we take the entire path.
bool GetPathFromCFObject(CFTypeRef obj, std::string* output) {
  // Dealing with file system representations in Core Foundation is a pain;
  // cheat by bridging to Foundation types.
  id ns_obj = (__bridge id)obj;

  if (NSString* str = base::apple::ObjCCast<NSString>(ns_obj)) {
    output->assign(str.fileSystemRepresentation);
    return true;
  }
  if (NSURL* url = base::apple::ObjCCast<NSURL>(ns_obj)) {
    output->assign(url.path.fileSystemRepresentation);
    return true;
  }
  if (NSBundle* bundle = base::apple::ObjCCast<NSBundle>(ns_obj)) {
    output->assign(bundle.bundlePath.fileSystemRepresentation);
    return true;
  }
  return false;
}

// Extract the signature information from the mach-o or extended attributes.
void ExtractSignatureInfo(const base::FilePath& path,
                          ClientDownloadRequest_ImageHeaders* image_headers,
                          ClientDownloadRequest_SignatureInfo* signature) {
  scoped_refptr<BinaryFeatureExtractor> bfe = new BinaryFeatureExtractor();

  // If Chrome ever opts into the macOS "kill" semantics, this
  // call has to change. `ExtractImageFeatures` maps the file, which will
  // cause Chrome to be killed before it can report on the invalid file.
  // This call will need to read(2) the binary into a buffer.
  if (!bfe->ExtractImageFeatures(path, BinaryFeatureExtractor::kDefaultOptions,
                                 image_headers,
                                 signature->mutable_signed_data())) {
    // If this is not a mach-o file, search inside the extended attributes.
    for (const char* attr : xattrs) {
      ssize_t size = getxattr(path.value().c_str(), attr, nullptr, 0, 0, 0);
      if (size >= 0) {
        std::vector<uint8_t> xattr_data(size);
        ssize_t post_size = getxattr(path.value().c_str(), attr, &xattr_data[0],
                                     xattr_data.size(), 0, 0);
        if (post_size >= 0) {
          xattr_data.resize(post_size);
          ClientDownloadRequest_ExtendedAttr* xattr_msg =
              signature->add_xattr();
          xattr_msg->set_key(attr);
          xattr_msg->set_value(xattr_data.data(), xattr_data.size());
        }
      }
    }
  }
}

// Process the CFErrorRef information about any files that were altered.
void ReportAlteredFiles(
    CFTypeRef detail,
    const base::FilePath& bundle_path,
    ClientIncidentReport_IncidentData_BinaryIntegrityIncident* incident) {
  if (CFArrayRef array = base::apple::CFCast<CFArrayRef>(detail)) {
    for (CFIndex i = 0; i < CFArrayGetCount(array); ++i) {
      ReportAlteredFiles(CFArrayGetValueAtIndex(array, i), bundle_path,
                         incident);
    }
  } else {
    std::string path_str;
    if (!GetPathFromCFObject(detail, &path_str)) {
      return;
    }
    std::string relative_path;
    base::FilePath path(path_str);
    // If the relative path calculation fails, at least take the basename.
    if (!MacSignatureEvaluator::GetRelativePathComponent(bundle_path, path,
                                                         &relative_path)) {
      relative_path = path.BaseName().value();
    }

    // Filter out certain noise reports on the client side.
    if (base::EndsWith(relative_path, kStringsFile,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return;
    }

    ClientIncidentReport_IncidentData_BinaryIntegrityIncident_ContainedFile*
        contained_file = incident->add_contained_file();
    contained_file->set_relative_path(relative_path);
    ExtractSignatureInfo(base::FilePath(path_str),
                         contained_file->mutable_image_headers(),
                         contained_file->mutable_signature());
  }
}

}  // namespace

MacSignatureEvaluator::MacSignatureEvaluator(
    const base::FilePath& signed_object_path)
    : path_(signed_object_path), has_requirement_(false) {}

MacSignatureEvaluator::MacSignatureEvaluator(
    const base::FilePath& signed_object_path,
    const std::string& requirement)
    : path_(signed_object_path),
      requirement_str_(requirement),
      has_requirement_(true) {}

MacSignatureEvaluator::~MacSignatureEvaluator() = default;

bool MacSignatureEvaluator::GetRelativePathComponent(
    const base::FilePath& parent,
    const base::FilePath& child,
    std::string* out) {
  if (!parent.IsParent(child))
    return false;

  std::vector<base::FilePath::StringType> parent_components =
      parent.GetComponents();
  std::vector<base::FilePath::StringType> child_components =
      child.GetComponents();

  size_t i = 0;
  while (i < parent_components.size() &&
         child_components[i] == parent_components[i]) {
    ++i;
  }

  while (i < child_components.size()) {
    out->append(child_components[i]);
    if (++i < child_components.size())
      out->append("/");
  }
  return true;
}

bool MacSignatureEvaluator::Initialize() {
  base::apple::ScopedCFTypeRef<CFURLRef> code_url =
      base::apple::FilePathToCFURL(path_);
  if (!code_url)
    return false;

  if (SecStaticCodeCreateWithPath(code_url.get(), kSecCSDefaultFlags,
                                  code_.InitializeInto()) != errSecSuccess) {
    return false;
  }

  if (has_requirement_) {
    if (SecRequirementCreateWithString(
            base::SysUTF8ToCFStringRef(requirement_str_).get(),
            kSecCSDefaultFlags,
            requirement_.InitializeInto()) != errSecSuccess) {
      return false;
    }
  }
  return true;
}

bool MacSignatureEvaluator::PerformEvaluation(
    ClientIncidentReport_IncidentData_BinaryIntegrityIncident* incident) {
  DCHECK(incident->contained_file_size() == 0);
  base::apple::ScopedCFTypeRef<CFErrorRef> errors;
  OSStatus err = SecStaticCodeCheckValidityWithErrors(
      code_.get(), kSecCSCheckAllArchitectures, requirement_.get(),
      errors.InitializeInto());
  if (err == errSecSuccess)
    return true;
  // Add the signature of the main binary to the incident report.
  incident->set_file_basename(path_.BaseName().value());
  incident->set_sec_error(err);
  // We heuristically detect if we are in a bundle or not by checking if
  // the main executable is different from the path_.
  base::apple::ScopedCFTypeRef<CFDictionaryRef> info_dict;
  base::FilePath exec_path;
  if (SecCodeCopySigningInformation(code_.get(), kSecCSDefaultFlags,
                                    info_dict.InitializeInto()) ==
      errSecSuccess) {
    CFURLRef exec_url = base::apple::CFCastStrict<CFURLRef>(
        CFDictionaryGetValue(info_dict.get(), kSecCodeInfoMainExecutable));
    if (!exec_url)
      return false;

    exec_path =
        base::apple::NSURLToFilePath(base::apple::CFToNSPtrCast(exec_url));
    if (exec_path != path_) {
      ReportAlteredFiles(exec_url, path_, incident);
    } else {
      // We may be examining a flat executable file, so extract any signature.
      ExtractSignatureInfo(path_, incident->mutable_image_headers(),
                           incident->mutable_signature());
    }
  }

  if (errors) {
    base::apple::ScopedCFTypeRef<CFDictionaryRef> info(
        CFErrorCopyUserInfo(errors.get()));
    static const CFStringRef keys[] = {
        kSecCFErrorResourceAltered, kSecCFErrorResourceMissing,
    };
    for (CFStringRef key : keys) {
      if (CFTypeRef detail = CFDictionaryGetValue(info.get(), key)) {
        ReportAlteredFiles(detail, path_, incident);
      }
    }
  }

  // Some resource violations (localizations) are skipped, so if the error is
  // that a sealed resource is missing or invalid, and there are no contained
  // files aside from the main executable, do not send the report.
  if (err == errSecCSBadResource && incident->contained_file_size() == 1) {
    if (base::EndsWith(exec_path.value(),
                       incident->contained_file(0).relative_path(),
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }

  return false;
}

}  // namespace safe_browsing
