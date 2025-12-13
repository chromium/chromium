// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/system_memory/memory_info_provider.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using api::system_memory::MemoryInfo;

class MockMemoryInfoProviderImpl : public MemoryInfoProvider {
 public:
  MockMemoryInfoProviderImpl() = default;
  MockMemoryInfoProviderImpl(const MockMemoryInfoProviderImpl&) = delete;
  MockMemoryInfoProviderImpl& operator=(const MockMemoryInfoProviderImpl&) =
      delete;

  bool QueryInfo() override {
    info_.capacity = 4096;
    info_.available_capacity = 1024;
    return true;
  }

 private:
  ~MockMemoryInfoProviderImpl() override = default;
};

// Desktop android only supports manifest v3 and later, don't need to run
// tests for MV2 extensions.
#if !BUILDFLAG(IS_ANDROID)
using ContextType = extensions::browser_test_util::ContextType;

class SystemMemoryApiTest : public ExtensionApiTest,
                            public testing::WithParamInterface<ContextType> {
 public:
  SystemMemoryApiTest() : ExtensionApiTest(GetParam()) {}
  ~SystemMemoryApiTest() override = default;
  SystemMemoryApiTest(const SystemMemoryApiTest&) = delete;
  SystemMemoryApiTest& operator=(const SystemMemoryApiTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         SystemMemoryApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         SystemMemoryApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(SystemMemoryApiTest, Memory) {
  scoped_refptr<MemoryInfoProvider> provider = new MockMemoryInfoProviderImpl;
  // The provider is owned by the single MemoryInfoProvider instance.
  MemoryInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("system_memory")) << message_;
}
#endif

class SystemMemoryApiMV3Test : public ExtensionApiTest {
 public:
  SystemMemoryApiMV3Test() = default;
  ~SystemMemoryApiMV3Test() override = default;
  SystemMemoryApiMV3Test(const SystemMemoryApiMV3Test&) = delete;
  SystemMemoryApiMV3Test& operator=(const SystemMemoryApiMV3Test&) = delete;
};

// Test memory api for MV3 extension.
IN_PROC_BROWSER_TEST_F(SystemMemoryApiMV3Test, Memory) {
  scoped_refptr<MemoryInfoProvider> provider = new MockMemoryInfoProviderImpl;
  // The provider is owned by the single MemoryInfoProvider instance.
  MemoryInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("system_memory/mv3")) << message_;
}

}  // namespace extensions
