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

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/mach_o_image_reader_mac.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// OS X code signing data can be stored in extended attributes as well. This is
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

// Convenience function to get the appropriate path from a variety of NSObject
// types. For resources, code signing seems to give back an NSURL in which
// the path is relative to the bundle root. So in this case, we take the
// relative component, otherwise we take the entire path.
bool GetPathFromNSObject(id obj, std::string* output) {
  if (NSString* str = base::mac::ObjCCast<NSString>(obj)) {
    output->assign([str fileSystemRepresentation]);
    return true;
  }
  if (NSURL* url = base::mac::ObjCCast<NSURL>(obj)) {
    output->assign([[url path] fileSystemRepresentation]);
    return true;
  }
  if (NSBundle* bundle = base::mac::ObjCCast<NSBundle>(obj)) {
    output->assign([[bundle bundlePath] fileSystemRepresentation]);
    return true;
  }
  return false;
}

// Extract the signature information from the mach-o or extended attributes.
void ExtractSignatureInfo(const base::FilePath& path,
                          ClientDownloadRequest_ImageHeaders* image_headers,
                          ClientDownloadRequest_SignatureInfo* signature) {
  scoped_refptr<BinaryFeatureExtractor> bfe = new BinaryFeatureExtractor();

  // If Chrome ever opts into the OS X "kill" semantics, this
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

// Process the NSError information about any files that were altered.
void ReportAlteredFiles(
    id detail,
    const base::FilePath& bundle_path,
    ClientIncidentReport_IncidentData_BinaryIntegrityIncident* incident) {
  if (NSArray* arr = base::mac::ObjCCast<NSArray>(detail)) {
    for (id obj in arr)
      ReportAlteredFiles(obj, bundle_path, incident);
  } else {
    std::string path_str;
    if (!GetPathFromNSObject(detail, &path_str))
      return;
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
    : path_(signed_object_path),
      requirement_str_(),
      has_requirement_(false),
      code_(nullptr),
      requirement_(nullptr) {}

MacSignatureEvaluator::MacSignatureEvaluator(
    const base::FilePath& signed_object_path,
    const std::string& requirement)
    : path_(signed_object_path),
      requirement_str_(requirement),
      has_requirement_(true),
      code_(nullptr),
      requirement_(nullptr) {}

MacSignatureEvaluator::~MacSignatureEvaluator() {}

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
  base::scoped_nsobject<NSURL> code_url([[NSURL alloc]
      initFileURLWithPath:base::SysUTF8ToNSString(path_.value())]);
  if (!code_url)
    return false;

  if (SecStaticCodeCreateWithPath(base::mac::NSToCFCast(code_url.get()),
                                  kSecCSDefaultFlags,
                                  code_.InitializeInto()) != errSecSuccess) {
    return false;
  }

  if (has_requirement_) {
    if (SecRequirementCreateWithString(
            base::mac::NSToCFCast(base::SysUTF8ToNSString(requirement_str_)),
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
  base::ScopedCFTypeRef<CFErrorRef> errors;
  OSStatus err = SecStaticCodeCheckValidityWithErrors(
      code_, kSecCSCheckAllArchitectures, requirement_,
      errors.InitializeInto());
  if (err == errSecSuccess)
    return true;
  // Add the signature of the main binary to the incident report.
  incident->set_file_basename(path_.BaseName().value());
  incident->set_sec_error(err);
  // We heuristically detect if we are in a bundle or not by checking if
  // the main executable is different from the path_.
  base::ScopedCFTypeRef<CFDictionaryRef> info_dict;
  base::FilePath exec_path;
  if (SecCodeCopySigningInformation(code_, kSecCSDefaultFlags,
                                    info_dict.InitializeInto()) ==
      errSecSuccess) {
    CFURLRef exec_url = base::mac::CFCastStrict<CFURLRef>(
        CFDictionaryGetValue(info_dict, kSecCodeInfoMainExecutable));
    if (!exec_url)
      return false;

    exec_path = base::FilePath(
        [[base::mac::CFToNSCast(exec_url) path] fileSystemRepresentation]);
    if (exec_path != path_) {
      ReportAlteredFiles(base::mac::CFToNSCast(exec_url), path_, incident);
    } else {
      // We may be examing a flat executable file, so extract any signature.
      ExtractSignatureInfo(path_, incident->mutable_image_headers(),
                           incident->mutable_signature());
    }
  }

  if (errors) {
    NSDictionary* info = [base::mac::CFToNSCast(errors.get()) userInfo];
    static const CFStringRef keys[] = {
        kSecCFErrorResourceAltered, kSecCFErrorResourceMissing,
    };
    for (CFStringRef key : keys) {
      if (id detail = info[base::mac::CFToNSCast(key)])
        ReportAlteredFiles(detail, path_, incident);
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
