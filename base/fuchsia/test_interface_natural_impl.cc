// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/test_interface_natural_impl.h"

#include <lib/async/default.h>

#include <utility>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TestInterfaceNaturalImpl::TestInterfaceNaturalImpl() = default;
TestInterfaceNaturalImpl::~TestInterfaceNaturalImpl() = default;

void TestInterfaceNaturalImpl::Add(AddRequest& request,
                                   AddCompleter::Sync& completer) {
  completer.Reply(request.a() + request.b());
}

fidl::Client<base_testfidl::TestInterface> CreateTestInterfaceClient(
    fidl::UnownedClientEnd<fuchsia_io::Directory> service_directory,
    const std::string& name) {
  auto client_end = fuchsia_component::ConnectAt<base_testfidl::TestInterface>(
      service_directory, name);
  EXPECT_TRUE(client_end.is_ok());
  fidl::Client client(std::move(*client_end), async_get_default_dispatcher());
  return client;
}

zx_status_t VerifyTestInterface(
    fidl::Client<base_testfidl::TestInterface>& client) {
  // Call the service and wait for response.
  RunLoop run_loop;
  zx_status_t result = ZX_ERR_INTERNAL;
  base::WeakPtrFactory<zx_status_t> weak_result(&result);

  client->Add({{2, 2}}).Then(
      [quit = run_loop.QuitClosure(), weak_result = weak_result.GetWeakPtr()](
          fidl::Result<base_testfidl::TestInterface::Add>& result) {
        if (result.is_ok()) {
          EXPECT_EQ(result.value(), 4);
        }
        if (weak_result) {
          *weak_result =
              result.is_error() ? result.error_value().status() : ZX_OK;
        }
        std::move(quit).Run();
      });

  run_loop.Run();
  return result;
}

}  // namespace base
