// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_interface_impl.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TestInterfaceImpl::TestInterfaceImpl() = default;
TestInterfaceImpl::~TestInterfaceImpl() = default;

void TestInterfaceImpl::Add(int32_t a, int32_t b, AddCallback callback) {
  callback(a + b);
}

zx_status_t VerifyTestInterface(
    fidl::InterfacePtr<testfidl::TestInterface>& ptr) {
  // Call the service and wait for response.
  RunLoop run_loop;
  zx_status_t result = ZX_ERR_INTERNAL;
  base::WeakPtrFactory<zx_status_t> weak_result(&result);

  ptr.set_error_handler(
      [quit = run_loop.QuitClosure(),
       weak_result = weak_result.GetWeakPtr()](zx_status_t status) {
        if (weak_result)
          *weak_result = status;
        std::move(quit).Run();
      });

  ptr->Add(2, 2,
           [quit = run_loop.QuitClosure(),
            weak_result = weak_result.GetWeakPtr()](int32_t value) {
             EXPECT_EQ(value, 4);
             if (weak_result)
               *weak_result = ZX_OK;
             std::move(quit).Run();
           });

  run_loop.Run();

  // Reset error handler because the current one captures |run_loop| and
  // |error| references which are about to be destroyed.
  ptr.set_error_handler(nullptr);

  return result;
}

}  // namespace base
