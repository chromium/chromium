// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_HOST_MANIFEST_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_HOST_MANIFEST_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "extensions/common/url_pattern_set.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

class NativeMessagingHostManifest {
 public:
  enum HostInterface {
    HOST_INTERFACE_STDIO,
  };

  ~NativeMessagingHostManifest();

  // Verifies that the name is valid. Valid names must match regular expression
  // "([a-z0-9_]+.)*[a-z0-9_]+".
  static bool IsValidName(const std::string& name);

  // Load manifest file from |file_path|.
  static std::unique_ptr<NativeMessagingHostManifest> Load(
      const base::FilePath& file_path,
      std::string* error_message);

  const std::string& name() const { return name_; }
  const std::string& description() const { return description_; }
  HostInterface host_interface() const { return interface_; }
  const base::FilePath& path() const { return path_; }
  const URLPatternSet& allowed_origins() const { return allowed_origins_; }
  bool supports_native_initiated_connections() const {
    return supports_native_initiated_connections_;
  }

 private:
  NativeMessagingHostManifest();

  // Parses manifest |dictionary|. In case of an error sets |error_message| and
  // returns false.
  bool Parse(base::DictionaryValue* dictionary, std::string* error_message);

  std::string name_;
  std::string description_;
  HostInterface interface_;
  base::FilePath path_;
  URLPatternSet allowed_origins_;
  bool supports_native_initiated_connections_ = false;

  DISALLOW_COPY_AND_ASSIGN(NativeMessagingHostManifest);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_HOST_MANIFEST_H_
