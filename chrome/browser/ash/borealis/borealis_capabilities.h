// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CAPABILITIES_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CAPABILITIES_H_

#include "components/exo/capabilities.h"

namespace borealis {

// Controls what specialized behaviours the exo server will have when dealing
// with borealis clients.
class BorealisCapabilities : public exo::Capabilities {
 public:
  ~BorealisCapabilities() override;

  // exo::Capabilities overrides:
  std::string GetSecurityContext() const override;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CAPABILITIES_H_
