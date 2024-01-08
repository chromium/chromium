// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALL_URL_HANDLER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALL_URL_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"

namespace borealis {

// Registers the handler for the chromeos-steam://install URL.
class BorealisInstallUrlHandler {
 public:
  explicit BorealisInstallUrlHandler(Profile* profile);

 private:
  void RegisterHandler();

  raw_ptr<Profile> profile_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_INSTALL_URL_HANDLER_H_
