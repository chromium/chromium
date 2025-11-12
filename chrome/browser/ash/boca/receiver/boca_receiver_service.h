// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_H_
#define CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace ash {

// Service responsible for managing Boca receiver features.
class BocaReceiverService : public KeyedService {
 public:
  BocaReceiverService();

  BocaReceiverService(const BocaReceiverService&) = delete;
  BocaReceiverService& operator=(const BocaReceiverService&) = delete;

  ~BocaReceiverService() override;

  // KeyedService:
  void Shutdown() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_RECEIVER_BOCA_RECEIVER_SERVICE_H_
