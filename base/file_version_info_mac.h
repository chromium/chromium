// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_MAC_H_
#define BASE_FILE_VERSION_INFO_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <string>

#include "base/file_version_info.h"
#include "base/mac/scoped_nsobject.h"

@class NSBundle;

class FileVersionInfoMac : public FileVersionInfo {
 public:
  explicit FileVersionInfoMac(NSBundle *bundle);
  FileVersionInfoMac(const FileVersionInfoMac&) = delete;
  FileVersionInfoMac& operator=(const FileVersionInfoMac&) = delete;
  ~FileVersionInfoMac() override;

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  std::u16string company_name() override;
  std::u16string company_short_name() override;
  std::u16string product_name() override;
  std::u16string product_short_name() override;
  std::u16string internal_name() override;
  std::u16string product_version() override;
  std::u16string special_build() override;
  std::u16string original_filename() override;
  std::u16string file_description() override;
  std::u16string file_version() override;

 private:
  // Returns a std::u16string value for a property name.
  // Returns the empty string if the property does not exist.
  std::u16string GetString16Value(CFStringRef name);

  base::scoped_nsobject<NSBundle> bundle_;
};

#endif  // BASE_FILE_VERSION_INFO_MAC_H_
