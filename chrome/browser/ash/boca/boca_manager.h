// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_H_
#define CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {
// Manages boca main business logic.
class BocaManager : public KeyedService {
 public:
  static BocaManager* GetForProfile(Profile* profile);

  explicit BocaManager(Profile* profile);
  ~BocaManager() override;

};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_BOCA_MANAGER_H_
