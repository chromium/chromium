// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_publisher.h"

#include <lib/fidl/cpp/binding_set.h>

#include "base/fuchsia/service_directory_test_base.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class ScopedServicePublisherTest : public ServiceDirectoryTestBase {};

TEST_F(ScopedServicePublisherTest, ConstructorPublishesService) {
  // Remove the default service binding.
  service_binding_.reset();

  // Create bindings and register using a publisher instance.
  fidl::BindingSet<testfidl::TestInterface> bindings;
  ScopedServicePublisher<testfidl::TestInterface> publisher(
      outgoing_directory_.get(), bindings.GetHandler(&test_service_));
  auto client = public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&client, ZX_OK);
}

TEST_F(ScopedServicePublisherTest, DestructorRemovesService) {
  // Remove the default service binding.
  service_binding_.reset();

  fidl::BindingSet<testfidl::TestInterface> bindings;
  {
    ScopedServicePublisher<testfidl::TestInterface> publisher(
        outgoing_directory_.get(), bindings.GetHandler(&test_service_));
  }
  // Once the publisher leaves scope, the service shouldn't be available.
  auto new_client =
      public_service_directory_->Connect<testfidl::TestInterface>();
  VerifyTestInterface(&new_client, ZX_ERR_PEER_CLOSED);
}

}  // namespace base
