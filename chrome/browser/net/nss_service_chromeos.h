// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_H_
#define CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

// ChromeOS Ash service that manages initialization of NSS and the per-profile
// certificate database.
class NssServiceChromeOS : public KeyedService {
 public:
  explicit NssServiceChromeOS(Profile* profile);
  NssServiceChromeOS(const NssServiceChromeOS&) = delete;
  NssServiceChromeOS& operator=(const NssServiceChromeOS&) = delete;
  ~NssServiceChromeOS() override;

 private:
  // TODO(https://cbug.com/1018972):  Move ownership of
  // NSSCertDatabaseChromeOSManager to this class.
};

#endif  // CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_H_
