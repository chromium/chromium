// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_EVALUATOR_MAC_H_
#define CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_EVALUATOR_MAC_H_

#include <Security/Security.h>

#include <string>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"

namespace safe_browsing {

// Wraps the macOS SecStaticCode API, to evaluate a given file object
// with a given code requirement, and produce a list of incident reports
// for files that fail code signature validity checks.
class MacSignatureEvaluator {
 public:
  explicit MacSignatureEvaluator(const base::FilePath& signed_object_path);

  // The requirement string must be a valid "Code Signing Requirement Language
  // string, which describes the identity of the signer.
  MacSignatureEvaluator(const base::FilePath& signed_object_path,
                        const std::string& requirement);

  MacSignatureEvaluator(const MacSignatureEvaluator&) = delete;
  MacSignatureEvaluator& operator=(const MacSignatureEvaluator&) = delete;

  ~MacSignatureEvaluator();

  // Creates the static code object and requirement string, and returns
  // true if the object creation succeeds, else false.
  bool Initialize();

  // Evaluate the signature and return a list of any binary integrity incident
  // reports. Returns true if and only if the signed code object is valid.
  bool PerformEvaluation(
      ClientIncidentReport_IncidentData_BinaryIntegrityIncident* incident);

  // Returns relative path component between a parent and a child.
  // For example, /foo/bar and /foo/bar/y returns y. Note that
  // this knows nothing about symlinks. Exposed for testing.
  static bool GetRelativePathComponent(const base::FilePath& parent,
                                       const base::FilePath& child,
                                       std::string* out);

 private:
  // The path to the code object on disk.
  base::FilePath path_;

  // A Code Signing Requirement string.
  std::string requirement_str_;

  // Records whether or not a requirement string was specified.
  bool has_requirement_;

  // The static code object constructed from the code object on disk.
  base::apple::ScopedCFTypeRef<SecStaticCodeRef> code_;

  // The requirement object constructed from the requirement string.
  base::apple::ScopedCFTypeRef<SecRequirementRef> requirement_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_EVALUATOR_MAC_H_
