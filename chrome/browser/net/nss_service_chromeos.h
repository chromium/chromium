// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_H_
#define CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/net/nss_context.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// ChromeOS Ash service that owns and initializates the per-Profile certificate
// database.
class NssServiceChromeOS : public KeyedService {
 public:
  explicit NssServiceChromeOS(Profile* profile);
  NssServiceChromeOS(const NssServiceChromeOS&) = delete;
  NssServiceChromeOS& operator=(const NssServiceChromeOS&) = delete;
  ~NssServiceChromeOS() override;

  // Returns an NssCertDatabaseGetter that may only be invoked on the IO thread.
  // To avoid UAF, the getter must be immediately posted to the IO thread and
  // then invoked.  While the returned getter must be invoked on the IO thread,
  // this method itself may only be invoked on the UI thread, where the
  // NssServiceChromeOS lives.
  NssCertDatabaseGetter CreateNSSCertDatabaseGetterForIOThread();

 private:
  // Owns and manages access to the net::NSSCertDatabaseChromeOS.
  class NSSCertDatabaseChromeOSManager;

  // Created on the UI thread, but after that, initialized, accessed, and
  // destroyed exclusively on the IO thread.
  std::unique_ptr<NSSCertDatabaseChromeOSManager> nss_cert_database_manager_;
};

#endif  // CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_H_
