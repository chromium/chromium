// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ANNOTATIONS_ANNOTATION_CONTROL_PROVIDER_H_
#define CHROME_BROWSER_POLICY_ANNOTATIONS_ANNOTATION_CONTROL_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/policy/annotations/annotation_control.h"

namespace policy {

// `AnnotationControlProvider` initializes and returns a map of network
// annotation hashcodes to their corresponding `AnnotationControl` objects,
// which can be consumed to determine whether a network annotation should be
// disabled based on current policy values.
class AnnotationControlProvider {
 public:
  AnnotationControlProvider();
  ~AnnotationControlProvider();

  // Returns a map of `AnnotationControl` objects, keyed by network annotation
  // hashcode (string). See implementation of `Load()` for more details on the
  // contents of this map.
  base::flat_map<std::string, AnnotationControl> GetControls();

 private:
  // Initializes `AnnotationControl` objects. Prefer lazy initialization to
  // reduce wasted resources.
  void Load();

  base::flat_map<std::string, AnnotationControl> annotation_controls_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ANNOTATIONS_ANNOTATION_CONTROL_PROVIDER_H_
