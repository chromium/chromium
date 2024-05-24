// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_js_callstacks.h"

#include "extensions/common/stack_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

class ExtensionJSCallStacksTest : public ::testing::Test {
 protected:
  ExtensionJSCallStacksTest() = default;
  extensions::StackTrace CreateStackTrace(unsigned int seed) {
    extensions::StackTrace stack_trace = {
        {seed + 1, seed + 1, u"foo1.js", u"Func1"},
        {seed + 2, seed + 2, u"foo2.js", u"Func2"},
        {seed + 3, seed + 3, u"foo3.js", u"Func3"},
        {seed + 4, seed + 4, u"foo4.js", u"Func4"},
        {seed + 5, seed + 5, u"foo5.js", u"Func5"}};
    return stack_trace;
  }

  ExtensionJSCallStacks js_callstacks_;
};

TEST_F(ExtensionJSCallStacksTest, StartsWithEmptyStore) {
  EXPECT_EQ(0u, js_callstacks_.NumCallStacks());
}

TEST_F(ExtensionJSCallStacksTest, AddsCallStacks) {
  // Ignores empty callstack.
  js_callstacks_.Add(extensions::StackTrace());
  EXPECT_EQ(0u, js_callstacks_.NumCallStacks());

  // Adds valid callstack.
  extensions::StackTrace stack_trace = CreateStackTrace(0u);
  js_callstacks_.Add(stack_trace);
  EXPECT_EQ(1u, js_callstacks_.NumCallStacks());

  // Adding a duplicate callstack has no effect.
  js_callstacks_.Add(stack_trace);
  EXPECT_EQ(1u, js_callstacks_.NumCallStacks());

  // Add up to the max callstacks allowed and
  // verify that they are all added successfully.
  unsigned int max_stacks = js_callstacks_.MaxCallStacks();
  for (unsigned int i = 1; i < max_stacks; i++) {
    js_callstacks_.Add(CreateStackTrace(i));
  }
  EXPECT_EQ(max_stacks, js_callstacks_.NumCallStacks());

  // Adding 1 more than max allowed should have no effect.
  js_callstacks_.Add(CreateStackTrace(max_stacks));
  EXPECT_EQ(max_stacks, js_callstacks_.NumCallStacks());
}

TEST_F(ExtensionJSCallStacksTest, GetsCallStacks) {
  // Add up to the max callstacks allowed.
  // Save these added stacks in a vector for later comparison with
  // the retrieved data.
  unsigned int max_stacks = js_callstacks_.MaxCallStacks();
  std::vector<extensions::StackTrace> expected_stacks;
  for (unsigned int i = 0; i < max_stacks; i++) {
    extensions::StackTrace stack = CreateStackTrace(i);
    js_callstacks_.Add(stack);
    expected_stacks.push_back(std::move(stack));
  }
  EXPECT_EQ(max_stacks, js_callstacks_.NumCallStacks());

  // Retrieve the callstacks.
  RepeatedPtrField<SignalInfoJSCallStack> siginfo_callstacks =
      js_callstacks_.GetAll();
  // Verify that the stacks received are the same as those that were
  // added. Do this by verifying that:
  // - the number of stacks received is the same as that added.
  // - each stack retrieved matches a corresponding expected stack.
  ASSERT_EQ(static_cast<int>(max_stacks), siginfo_callstacks.size());
  for (unsigned int s = 0; s < max_stacks; s++) {
    extensions::StackTrace stack_trace =
        ExtensionJSCallStacks::ToExtensionsStackTrace(siginfo_callstacks[s]);
    // Verify that exactly one stack was matched and removed.
    EXPECT_EQ(1u, std::erase(expected_stacks, stack_trace));
  }
  // Each matched stack is removed, so we should end up with an empty
  // vector at the end.
  EXPECT_TRUE(expected_stacks.empty());
}

}  // namespace
}  // namespace safe_browsing
