// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_MANAGER_MOCK_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_MANAGER_MOCK_H_

#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {

class BorealisContextManagerMock : public BorealisContextManager {
 public:
  BorealisContextManagerMock();

  ~BorealisContextManagerMock() override;

  MOCK_METHOD(void,
              StartBorealis,
              (BorealisContextManager::ResultCallback),
              ());

  MOCK_METHOD(bool, IsRunning, (), ());

  MOCK_METHOD(void,
              ShutDownBorealis,
              (base::OnceCallback<void(BorealisShutdownResult)>),
              ());
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CONTEXT_MANAGER_MOCK_H_
