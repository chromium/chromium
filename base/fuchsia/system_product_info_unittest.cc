// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fidl/fuchsia.buildinfo/cpp/fidl.h>
#include <fidl/fuchsia.hwinfo/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fidl/cpp/wire/connect_service.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/system_info.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class FakeHardwareInfoProduct : public fidl::Server<fuchsia_hwinfo::Product> {
 public:
  FakeHardwareInfoProduct(std::string_view model,
                          std::string_view manufacturer,
                          sys::OutgoingDirectory* outgoing_services)
      : model_(model),
        manufacturer_(manufacturer),
        binding_(outgoing_services, this) {}
  FakeHardwareInfoProduct(const FakeHardwareInfoProduct&) = delete;
  FakeHardwareInfoProduct& operator=(const FakeHardwareInfoProduct&) = delete;
  ~FakeHardwareInfoProduct() override = default;

  void GetInfo(GetInfoCompleter::Sync& completer) override {
    completer.Reply(fuchsia_hwinfo::ProductInfo{{
        .model = model_,
        .manufacturer = manufacturer_,
    }});
  }

 private:
  std::string model_;
  std::string manufacturer_;
  ScopedNaturalServiceBinding<fuchsia_hwinfo::Product> binding_;
};

}  // namespace

// Uses a fake "fuchsia.hwinfo.Product" implementation.
// clears the cached ProductInfo to ensure that each test starts with no cached
// ProductInfo and that subsequent tests runs do not use fake values.
class ProductInfoTest : public testing::Test {
 protected:
  ProductInfoTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO),
        thread_("ProductInfo Retrieval Thread") {
    thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    ClearCachedSystemInfoForTesting();
    component_context_.AddService(
        fidl::DiscoverableProtocolName<fuchsia_buildinfo::Provider>);
  }
  ~ProductInfoTest() override { ClearCachedSystemInfoForTesting(); }

  // Fetch the product info in a separate thread, while servicing the
  // FIDL fake implementation on the main thread.
  fuchsia_hwinfo::ProductInfo GetProductInfoViaTask() {
    fuchsia_hwinfo::ProductInfo product_info;
    base::RunLoop run_loop;
    thread_.task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&GetProductInfo),
        base::BindOnce(
            [](base::RunLoop& run_loop,
               fuchsia_hwinfo::ProductInfo& product_info,
               fuchsia_hwinfo::ProductInfo result) {
              product_info = std::move(result);
              run_loop.Quit();
            },
            std::ref(run_loop), std::ref(product_info)));
    run_loop.Run();
    return product_info;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestComponentContextForProcess component_context_;
  base::Thread thread_;
};

using ProductInfoDeathTest = ProductInfoTest;

TEST_F(ProductInfoTest, GetProductInfoReturnsFakedValues) {
  FakeHardwareInfoProduct hwinfo_product_provider(
      "test.model", "test.manufacturer",
      component_context_.additional_services());

  const auto product_info = GetProductInfoViaTask();
  EXPECT_EQ(product_info.model().value(), "test.model");
  EXPECT_EQ(product_info.manufacturer().value(), "test.manufacturer");
}

TEST_F(ProductInfoTest, SystemServiceReturnsValidValues) {
  component_context_.AddService(
      fidl::DiscoverableProtocolName<fuchsia_hwinfo::Product>);

  const auto product_info = GetProductInfoViaTask();
  EXPECT_TRUE(product_info.model().has_value());
  EXPECT_FALSE(product_info.model()->empty());

  EXPECT_TRUE(product_info.manufacturer().has_value());
  EXPECT_FALSE(product_info.manufacturer()->empty());
}

// TODO(crbug.com/40103081): Re-enable once all clients
// provide this service.
TEST_F(ProductInfoDeathTest, DISABLED_DcheckOnServiceNotPresent) {
  EXPECT_DCHECK_DEATH_WITH(GetProductInfoViaTask(), "ZX_ERR_PEER_CLOSED");
}

}  // namespace base
