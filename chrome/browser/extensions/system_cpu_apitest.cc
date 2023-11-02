// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/system_cpu/cpu_info_provider.h"

namespace extensions {

using api::system_cpu::CpuInfo;

class MockCpuInfoProviderImpl : public CpuInfoProvider {
 public:
  MockCpuInfoProviderImpl() = default;
  MockCpuInfoProviderImpl(const MockCpuInfoProviderImpl&) = delete;
  MockCpuInfoProviderImpl& operator=(const MockCpuInfoProviderImpl&) = delete;

  bool QueryInfo() override {
    info_.num_of_processors = 4;
    info_.arch_name = "x86";
    info_.model_name = "unknown";

    info_.features.clear();
    info_.features.push_back("mmx");
    info_.features.push_back("avx");

    info_.processors.clear();
    info_.processors.emplace_back(api::system_cpu::ProcessorInfo());
    info_.processors[0].usage.kernel = 1;
    info_.processors[0].usage.user = 2;
    info_.processors[0].usage.idle = 3;
    info_.processors[0].usage.total = 6;

    // The fractional part of these values should be exactly represented as
    // floating points to avoid rounding errors.
    info_.temperatures = {30.125, 40.0625};
    return true;
  }

 private:
  ~MockCpuInfoProviderImpl() override = default;
};

using ContextType = ExtensionBrowserTest::ContextType;

class SystemCpuApiTest : public ExtensionApiTest,
                         public testing::WithParamInterface<ContextType> {
 public:
  SystemCpuApiTest() : ExtensionApiTest(GetParam()) {}
  ~SystemCpuApiTest() override = default;
  SystemCpuApiTest(const SystemCpuApiTest&) = delete;
  SystemCpuApiTest& operator=(const SystemCpuApiTest&) = delete;
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         SystemCpuApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         SystemCpuApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(SystemCpuApiTest, Cpu) {
  scoped_refptr<CpuInfoProvider> provider = new MockCpuInfoProviderImpl;
  // The provider is owned by the single CpuInfoProvider instance.
  CpuInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunExtensionTest("system_cpu")) << message_;
}

}  // namespace extensions
