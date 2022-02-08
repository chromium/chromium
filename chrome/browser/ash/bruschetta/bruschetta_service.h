// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace bruschetta {

// A service to hold the separate modules that provide Bruschetta
// (third-party/generic VM) support within Chrome (files app integration, app
// service integration, etc).
class BruschettaService : public KeyedService {
 public:
  BruschettaService();
  ~BruschettaService() override;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_SERVICE_H_
