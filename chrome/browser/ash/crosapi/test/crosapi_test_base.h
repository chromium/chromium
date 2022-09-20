// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_

#include <memory>

#include "base/process/process.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

// This is to test the behavior of crosapi only on Ash-side.
class CrosapiTestBase : public ::testing::Test {
 public:
  CrosapiTestBase();
  ~CrosapiTestBase() override;

  CrosapiTestBase(const CrosapiTestBase&) = delete;
  CrosapiTestBase& operator=(const CrosapiTestBase&) = delete;

  // Launch Ash process and bind crosapi between two process.
  void SetUp() override;
  void TearDown() override;

 protected:
  mojo::Remote<mojom::Crosapi> remote_crosapi;

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  base::Process process_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
