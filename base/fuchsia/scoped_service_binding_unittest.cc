// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_binding.h"

#include "base/fuchsia/service_directory_test_base.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ScopedServiceBindingTest : public ServiceDirectoryTestBase {};

// Verifies that ScopedServiceBinding allows connection more than once.
TEST_F(ScopedServiceBindingTest, ConnectTwice) {
  auto stub = public_service_directory_->Connect<testfidl::TestInterface>();
  auto stub2 = public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_OK);
  VerifyTestInterface(&stub2, ZX_OK);
}

// Verifies that ScopedServiceBinding allows connection more than once.
TEST_F(ScopedServiceBindingTest, ConnectTwiceNewName) {
  const char kInterfaceName[] = "fuchsia.TestInterface2";

  ScopedServiceBinding<testfidl::TestInterface> new_service_binding(
      outgoing_directory_.get(), &test_service_, kInterfaceName);

  testfidl::TestInterfacePtr stub, stub2;
  public_service_directory_->Connect(
       kInterfaceName, stub.NewRequest().TakeChannel());
  public_service_directory_->Connect(
       kInterfaceName, stub2.NewRequest().TakeChannel());
  VerifyTestInterface(&stub, ZX_OK);
  VerifyTestInterface(&stub2, ZX_OK);
}

// Verifies that ScopedSingleClientServiceBinding allows a different name.
TEST_F(ScopedServiceBindingTest, SingleClientConnectNewName) {
  const char kInterfaceName[] = "fuchsia.TestInterface2";
  auto service_binding_new_name_ = std::make_unique<
      ScopedSingleClientServiceBinding<testfidl::TestInterface>>(
          outgoing_directory_.get(), &test_service_, kInterfaceName);

  testfidl::TestInterfacePtr stub;
  public_service_directory_->Connect(kInterfaceName,
                                     stub.NewRequest().TakeChannel());
  VerifyTestInterface(&stub, ZX_OK);
}

// Verify that if we connect twice to a prefer-new bound service, the existing
// connection gets closed.
TEST_F(ScopedServiceBindingTest, SingleClientPreferNew) {
  // Teardown the default multi-client binding and create a prefer-new one.
  service_binding_ = nullptr;
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kPreferNew>
      binding(outgoing_directory_.get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&existing_client, ZX_OK);

  // Connect the second client, so the existing one should be disconnected and
  // the new should be functional.
  auto new_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(existing_client);
  VerifyTestInterface(&new_client, ZX_OK);
}

// Verify that if we connect twice to a prefer-existing bound service, the new
// connection gets closed.
TEST_F(ScopedServiceBindingTest, SingleClientPreferExisting) {
  // Teardown the default multi-client binding and create a prefer-existing one.
  service_binding_ = nullptr;
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kPreferExisting>
      binding(outgoing_directory_.get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&existing_client, ZX_OK);

  // Connect the second client, then verify that the it gets closed and the
  // existing one remains functional.
  auto new_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_client);
  VerifyTestInterface(&existing_client, ZX_OK);
}

// Verify that the default single-client binding policy is prefer-new.
TEST_F(ScopedServiceBindingTest, SingleClientDefaultIsPreferNew) {
  // Teardown the default multi-client binding and create a prefer-new one.
  service_binding_ = nullptr;
  ScopedSingleClientServiceBinding<testfidl::TestInterface> binding(
      outgoing_directory_.get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&existing_client, ZX_OK);

  // Connect the second client, so the existing one should be disconnected and
  // the new should be functional.
  auto new_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(existing_client);
  VerifyTestInterface(&new_client, ZX_OK);
}

// Verify that we can publish a debug service.
TEST_F(ScopedServiceBindingTest, ConnectDebugService) {
  // Remove the public service binding.
  service_binding_.reset();

  // Publish the test service to the "debug" directory.
  ScopedServiceBinding<testfidl::TestInterface> debug_service_binding(
      outgoing_directory_->debug_dir(), &test_service_);

  auto debug_stub =
      debug_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&debug_stub, ZX_OK);

  auto release_stub =
      public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&release_stub, ZX_ERR_PEER_CLOSED);
}

TEST_F(ScopedServiceBindingTest, SingleBindingSetOnLastClientCallback) {
  service_binding_.reset();
  ScopedSingleClientServiceBinding<testfidl::TestInterface>
      single_service_binding(outgoing_directory_.get(), &test_service_);

  base::RunLoop run_loop;
  single_service_binding.SetOnLastClientCallback(run_loop.QuitClosure());

  auto current_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&current_client, ZX_OK);
  current_client.Unbind();

  run_loop.Run();
}

// Test the kConnectOnce option for ScopedSingleClientServiceBinding properly
// stops publishing the service after a first disconnect.
TEST_F(ScopedServiceBindingTest, ConnectOnce_OnlyFirstConnectionSucceeds) {
  // Teardown the default multi-client binding and create a connect-once one.
  service_binding_ = nullptr;
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kConnectOnce>
      binding(outgoing_directory_.get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&existing_client, ZX_OK);

  // Connect the second client, then verify that it gets closed and the existing
  // one remains functional.
  auto new_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_client);
  VerifyTestInterface(&existing_client, ZX_OK);

  // Disconnect the first client.
  existing_client.Unbind().TakeChannel().reset();
  RunLoop().RunUntilIdle();

  // Re-connect the second client, then verify that it gets closed.
  new_client = public_service_directory_->Connect<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_client);
}

class MultiUseBindingTest : public ScopedServiceBindingTest {
 public:
  MultiUseBindingTest() {
    service_binding_->SetOnLastClientCallback(
        BindRepeating(&MultiUseBindingTest::OnLastClient, Unretained(this)));
  }
  ~MultiUseBindingTest() override = default;

 protected:
  void OnLastClient() { disconnect_count_++; }

  int disconnect_count_ = 0;
};

// Test the last client callback is called every time the number of active
// clients reaches 0.
TEST_F(MultiUseBindingTest, MultipleLastClientCallback) {
  // Connect a client, verify it is functional.
  auto stub = public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_OK);

  // Disconnect the client, the callback should have been called once.
  stub.Unbind().TakeChannel().reset();
  RunLoop().RunUntilIdle();
  EXPECT_EQ(disconnect_count_, 1);

  // Re-connect the client, verify it is functional.
  stub = public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_OK);

  // Disconnect the client, the callback should have been called a second time.
  stub.Unbind().TakeChannel().reset();
  RunLoop().RunUntilIdle();
  EXPECT_EQ(disconnect_count_, 2);
}

}  // namespace base
