// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_

#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

// Base class for testing the behavior of crosapi on Ash-side only.
class CrosapiTestBase : public ::testing::Test {
 public:
  CrosapiTestBase();
  ~CrosapiTestBase() override;
  CrosapiTestBase(const CrosapiTestBase&) = delete;
  CrosapiTestBase& operator=(const CrosapiTestBase&) = delete;

  void SetUp() override;

 protected:
  mojo::Remote<mojom::Crosapi>& GetCrosapiRemote();
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CROSAPI_TEST_BASE_H_
