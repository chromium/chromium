// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_DESTINATION_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_DESTINATION_H_

#include "chrome/browser/enterprise/data_controls/component.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {
// DlpFileDestination represents the destination for file transfer. It has a
// field to save an component and one to save an url. Either one of them is set
// or none. The case of both values undefined is to be interpreted as the
// destination is within the MyFiles filesystem.
class DlpFileDestination {
 public:
  DlpFileDestination();
  explicit DlpFileDestination(const GURL& url);
  explicit DlpFileDestination(const data_controls::Component component);

  DlpFileDestination(const DlpFileDestination&);
  DlpFileDestination& operator=(const DlpFileDestination&);
  DlpFileDestination(DlpFileDestination&&);
  DlpFileDestination& operator=(DlpFileDestination&&);

  bool operator==(const DlpFileDestination&) const;
  bool operator!=(const DlpFileDestination&) const;
  bool operator<(const DlpFileDestination& other) const;
  bool operator<=(const DlpFileDestination& other) const;
  bool operator>(const DlpFileDestination& other) const;
  bool operator>=(const DlpFileDestination& other) const;

  ~DlpFileDestination();

  absl::optional<GURL> url() const;

  absl::optional<data_controls::Component> component() const;

  // Returns if the destination is in a local filesystem (any
  // `data_control::Destination` or MyFiles).
  bool IsFileSystem() const;

 private:
  // Destination url or destination path.
  absl::optional<GURL> url_;
  // Destination component.
  absl::optional<data_controls::Component> component_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_DESTINATION_H_
