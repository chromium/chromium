// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fuchsia/element_manager_impl.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include "base/command_line.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestElementManagerImpl : public testing::Test {
 public:
  TestElementManagerImpl()
      : element_manager_(base::ComponentContextForProcess()->outgoing().get(),
                         base::BindLambdaForTesting(
                             [&](const base::CommandLine& command_line) {
                               received_command_line_ = command_line;
                               return true;
                             })) {}

 protected:
  fuchsia::element::ManagerPtr GetElementManagerPtr() {
    return test_context_.published_services()
        ->Connect<fuchsia::element::Manager>();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::TestComponentContextForProcess test_context_;
  std::optional<base::CommandLine> received_command_line_;
  ElementManagerImpl element_manager_;
};

TEST_F(TestElementManagerImpl, TestCorrectSpec) {
  fuchsia::element::Spec spec;
  spec.set_component_url("fuchsia-pkg://fuchsia.com/chrome#meta/chrome.cm");

  auto element_manager = GetElementManagerPtr();
  base::RunLoop run_loop;
  std::optional<fuchsia::element::Manager_ProposeElement_Result>
      received_result;
  element_manager->ProposeElement(
      std::move(spec), {},
      [&](fuchsia::element::Manager_ProposeElement_Result result) {
        received_result = std::move(result);
        run_loop.Quit();
      });
  run_loop.Run();
  ASSERT_TRUE(received_result);
  EXPECT_FALSE(received_result->is_err());
  EXPECT_TRUE(received_command_line_);
}

TEST_F(TestElementManagerImpl, TestIncorrectSpec) {
  fuchsia::element::Spec spec;
  spec.set_component_url("foobar");

  auto element_manager = GetElementManagerPtr();
  base::RunLoop run_loop;
  std::optional<fuchsia::element::Manager_ProposeElement_Result>
      received_result;
  element_manager->ProposeElement(
      std::move(spec), {},
      [&](fuchsia::element::Manager_ProposeElement_Result result) {
        received_result = std::move(result);
        run_loop.Quit();
      });
  run_loop.Run();
  ASSERT_TRUE(received_result);
  EXPECT_TRUE(received_result->is_err());
  EXPECT_FALSE(received_command_line_);
}

}  // namespace
