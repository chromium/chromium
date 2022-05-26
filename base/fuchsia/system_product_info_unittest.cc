// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/system_info.h"

#include <fuchsia/buildinfo/cpp/fidl.h>
#include <fuchsia/hwinfo/cpp/fidl.h>
#include <fuchsia/hwinfo/cpp/fidl_test_base.h>
#include <memory>

#include "base/bind.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class FakeHardwareInfoProduct
    : public fuchsia::hwinfo::testing::Product_TestBase {
 public:
  FakeHardwareInfoProduct(const base::StringPiece model,
                          const base::StringPiece manufacturer,
                          sys::OutgoingDirectory* outgoing_services)
      : model_(model),
        manufacturer_(manufacturer),
        binding_(outgoing_services, this) {}
  FakeHardwareInfoProduct(const FakeHardwareInfoProduct&) = delete;
  FakeHardwareInfoProduct& operator=(const FakeHardwareInfoProduct&) = delete;
  ~FakeHardwareInfoProduct() override = default;

  // fuchsia::hwinfo::testing::Provider_TestBase implementation
  void GetInfo(GetInfoCallback callback) override {
    fuchsia::hwinfo::ProductInfo product_info;
    product_info.set_model(model_);
    product_info.set_manufacturer(manufacturer_);
    callback(std::move(product_info));
  }
  void NotImplemented_(const std::string& name) final {
    ADD_FAILURE() << "Unexpected call: " << name;
  }

 private:
  std::string model_;
  std::string manufacturer_;
  ScopedServiceBinding<fuchsia::hwinfo::Product> binding_;
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
    component_context_.AddService(fuchsia::buildinfo::Provider::Name_);
  }
  ~ProductInfoTest() override { ClearCachedSystemInfoForTesting(); }

  // Fetch the product info in a separate thread, while servicing the
  // FIDL fake implementation on the main thread.
  void FetchProductInfoAndWaitUntilCached() {
    base::RunLoop run_loop;
    thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, BindOnce(&FetchAndCacheSystemInfo), run_loop.QuitClosure());
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestComponentContextForProcess component_context_;
  base::Thread thread_;
};

using ProductInfoDeathTest = ProductInfoTest;

TEST_F(ProductInfoTest, GetCachedProductInfoReturnsFakedValues) {
  FakeHardwareInfoProduct hwinfo_product_provider(
      "test.model", "test.manufacturer",
      component_context_.additional_services());
  FetchProductInfoAndWaitUntilCached();

  const auto& product_info = GetCachedProductInfo();
  EXPECT_EQ(product_info.model(), "test.model");
  EXPECT_EQ(product_info.manufacturer(), "test.manufacturer");
}

TEST_F(ProductInfoDeathTest, DcheckOnGetWithoutFetch) {
  EXPECT_DCHECK_DEATH_WITH(
      GetCachedProductInfo(),
      "FetchAndCacheSystemInfo\\(\\) has not been called in this "
      "process");
}

TEST_F(ProductInfoTest, SystemServiceReturnsValidValues) {
  component_context_.AddService(fuchsia::hwinfo::Product::Name_);
  FetchProductInfoAndWaitUntilCached();

  const auto& product_info = GetCachedProductInfo();
  EXPECT_TRUE(product_info.has_model());
  EXPECT_FALSE(product_info.model().empty());

  EXPECT_TRUE(product_info.has_manufacturer());
  EXPECT_FALSE(product_info.manufacturer().empty());
}

TEST_F(ProductInfoDeathTest, DcheckOnServiceNotPresent) {
  EXPECT_DCHECK_DEATH_WITH(FetchProductInfoAndWaitUntilCached(),
                           "ZX_ERR_PEER_CLOSED");
}

}  // namespace base
