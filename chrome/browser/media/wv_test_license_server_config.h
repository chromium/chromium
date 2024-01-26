// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WV_TEST_LICENSE_SERVER_CONFIG_H_
#define CHROME_BROWSER_MEDIA_WV_TEST_LICENSE_SERVER_CONFIG_H_

#include <stdint.h>

#include "chrome/browser/media/test_license_server_config.h"

namespace base {
class FilePath;
}

// License configuration to run the Widevine test license server.
class WVTestLicenseServerConfig : public TestLicenseServerConfig {
 public:
  WVTestLicenseServerConfig();

  WVTestLicenseServerConfig(const WVTestLicenseServerConfig&) = delete;
  WVTestLicenseServerConfig& operator=(const WVTestLicenseServerConfig&) =
      delete;

  ~WVTestLicenseServerConfig() override;

  std::string GetServerURL() override;

  bool GetServerCommandLine(base::CommandLine* command_line) override;

  std::optional<base::EnvironmentMap> GetServerEnvironment() override;

  bool IsPlatformSupported() override;

 private:
  // Server port. The port value should be set by calling SelectServerPort().
  uint16_t port_;

  // Retrieves the path for the WV license server root:
  // third_party/widevine/test/license_server/
  void GetLicenseServerRootPath(base::FilePath* path);

  // Retrieves the path for the WV license server:
  // <license_server_root_path>/<platform>/
  void GetLicenseServerPath(base::FilePath* path);

  // Sets the server port to a randomly available port within a limited range.
  bool SelectServerPort();
};

#endif  // CHROME_BROWSER_MEDIA_WV_TEST_LICENSE_SERVER_CONFIG_H_
