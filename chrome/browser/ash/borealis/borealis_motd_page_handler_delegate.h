// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_MOTD_PAGE_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_MOTD_PAGE_HANDLER_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_page_handler.h"

namespace borealis {

class BorealisFeatures;
class BorealisInstaller;

class BorealisMOTDPageHandlerDelegate
    : public BorealisMOTDPageHandler::Delegate {
 public:
  // Passed `features` and `installer` must outlive this instance.
  BorealisMOTDPageHandlerDelegate(BorealisFeatures* features,
                                  BorealisInstaller* installer);

  // BorealisMOTDPageHandler::Delegate overrides
  bool IsBorealisInstalled() override;
  void UninstallBorealis() override;

 private:
  const raw_ref<BorealisFeatures> features_;
  const raw_ref<BorealisInstaller> installer_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_MOTD_PAGE_HANDLER_DELEGATE_H_
