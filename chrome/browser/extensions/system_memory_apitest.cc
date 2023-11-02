// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/system_memory/memory_info_provider.h"

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

using ContextType = ExtensionBrowserTest::ContextType;

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

}  // namespace extensions
