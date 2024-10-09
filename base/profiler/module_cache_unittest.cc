// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/module_cache.h"

#include <iomanip>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "base/debug/proc_maps_linux.h"
#endif

// Note: The special-case IS_CHROMEOS code inside GetDebugBasenameForModule to
// handle the interaction between that function and
// SetProcessTitleFromCommandLine() is tested in
// base/process/set_process_title_linux_unittest.cc due to dependency issues.

namespace base {
namespace {

int AFunctionForTest() {
  return 42;
}

// Provides a module that is guaranteed to be isolated from (and non-contiguous
// with) any other module, by placing the module in the middle of a block of
// heap memory.
class IsolatedModule : public ModuleCache::Module {
 public:
  explicit IsolatedModule(bool is_native = true)
      : is_native_(is_native), memory_region_(new char[kRegionSize]) {}

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override {
    // Place the module in the middle of the region.
    return reinterpret_cast<uintptr_t>(&memory_region_[kRegionSize / 4]);
  }

  std::string GetId() const override { return ""; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return kRegionSize / 2; }
  bool IsNative() const override { return is_native_; }

 private:
  static const int kRegionSize = 100;

  bool is_native_;
  std::unique_ptr<char[]> memory_region_;
};

// Provides a fake module with configurable base address and size.
class FakeModule : public ModuleCache::Module {
 public:
  FakeModule(uintptr_t base_address,
             size_t size,
             bool is_native = true,
             OnceClosure destruction_closure = OnceClosure())
      : base_address_(base_address),
        size_(size),
        is_native_(is_native),
        destruction_closure_runner_(std::move(destruction_closure)) {}

  FakeModule(const FakeModule&) = delete;
  FakeModule& operator=(const FakeModule&) = delete;

  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return ""; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return size_; }
  bool IsNative() const override { return is_native_; }

 private:
  uintptr_t base_address_;
  size_t size_;
  bool is_native_;
  ScopedClosureRunner destruction_closure_runner_;
};

// Utility function to add a single non-native module during test setup. Returns
// a pointer to the provided module.
const ModuleCache::Module* AddNonNativeModule(
    ModuleCache* cache,
    std::unique_ptr<const ModuleCache::Module> module) {
  const ModuleCache::Module* module_ptr = module.get();
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
  modules.push_back(std::move(module));
  cache->UpdateNonNativeModules({}, std::move(modules));
  return module_ptr;
}

#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_IOS) && !defined(ARCH_CPU_ARM64)) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN)
#define MAYBE_TEST(TestSuite, TestName) TEST(TestSuite, TestName)
#else
#define MAYBE_TEST(TestSuite, TestName) TEST(TestSuite, DISABLED_##TestName)
#endif

MAYBE_TEST(ModuleCacheTest, GetDebugBasename) {
  ModuleCache cache;
  const ModuleCache::Module* module =
      cache.GetModuleForAddress(reinterpret_cast<uintptr_t>(&AFunctionForTest));
  ASSERT_NE(nullptr, module);
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("libbase_unittests__library",
            module->GetDebugBasename().RemoveFinalExtension().value());
#elif BUILDFLAG(IS_POSIX)
  EXPECT_EQ("base_unittests", module->GetDebugBasename().value());
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ(L"base_unittests.exe.pdb", module->GetDebugBasename().value());
#endif
}

// Checks that ModuleCache returns the same module instance for
// addresses within the module.
MAYBE_TEST(ModuleCacheTest, LookupCodeAddresses) {
  uintptr_t ptr1 = reinterpret_cast<uintptr_t>(&AFunctionForTest);
  uintptr_t ptr2 = ptr1 + 1;
  ModuleCache cache;
  const ModuleCache::Module* module1 = cache.GetModuleForAddress(ptr1);
  const ModuleCache::Module* module2 = cache.GetModuleForAddress(ptr2);
  EXPECT_EQ(module1, module2);
  EXPECT_NE(nullptr, module1);
  EXPECT_GT(module1->GetSize(), 0u);
  EXPECT_LE(module1->GetBaseAddress(), ptr1);
  EXPECT_GT(module1->GetBaseAddress() + module1->GetSize(), ptr2);
}

MAYBE_TEST(ModuleCacheTest, LookupRange) {
  ModuleCache cache;
  auto to_inject = std::make_unique<IsolatedModule>();
  const ModuleCache::Module* module = to_inject.get();
  cache.AddCustomNativeModule(std::move(to_inject));

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() - 1));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress()));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress() +
                                              module->GetSize() - 1));
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() +
                                               module->GetSize()));
}

MAYBE_TEST(ModuleCacheTest, LookupNonNativeModule) {
  ModuleCache cache;
  const ModuleCache::Module* module =
      AddNonNativeModule(&cache, std::make_unique<IsolatedModule>(false));

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() - 1));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress()));
  EXPECT_EQ(module, cache.GetModuleForAddress(module->GetBaseAddress() +
                                              module->GetSize() - 1));
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(module->GetBaseAddress() +
                                               module->GetSize()));
}

MAYBE_TEST(ModuleCacheTest, LookupOverlaidNonNativeModule) {
  ModuleCache cache;

  auto native_module_to_inject = std::make_unique<IsolatedModule>();
  const ModuleCache::Module* native_module = native_module_to_inject.get();
  cache.AddCustomNativeModule(std::move(native_module_to_inject));

  // Overlay the native module with the non-native module, starting 8 bytes into
  // the native modules and ending 8 bytes before the end of the module.
  const ModuleCache::Module* non_native_module = AddNonNativeModule(
      &cache,
      std::make_unique<FakeModule>(native_module->GetBaseAddress() + 8,
                                   native_module->GetSize() - 16, false));

  EXPECT_EQ(native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress() - 1));
  EXPECT_EQ(non_native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress()));
  EXPECT_EQ(non_native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress() +
                                      non_native_module->GetSize() - 1));
  EXPECT_EQ(native_module,
            cache.GetModuleForAddress(non_native_module->GetBaseAddress() +
                                      non_native_module->GetSize()));
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesAdd) {
  ModuleCache cache;
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
  modules.push_back(std::make_unique<FakeModule>(1, 1, false));
  const ModuleCache::Module* module = modules.back().get();
  cache.UpdateNonNativeModules({}, std::move(modules));

  EXPECT_EQ(module, cache.GetModuleForAddress(1));
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesRemove) {
  ModuleCache cache;
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
  modules.push_back(std::make_unique<FakeModule>(1, 1, false));
  const ModuleCache::Module* module = modules.back().get();
  cache.UpdateNonNativeModules({}, std::move(modules));
  cache.UpdateNonNativeModules({module}, {});

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesRemoveModuleIsNotDestroyed) {
  bool was_destroyed = false;
  {
    ModuleCache cache;
    std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
    modules.push_back(std::make_unique<FakeModule>(
        1, 1, false,
        BindLambdaForTesting([&was_destroyed] { was_destroyed = true; })));
    const ModuleCache::Module* module = modules.back().get();
    cache.UpdateNonNativeModules({}, std::move(modules));
    cache.UpdateNonNativeModules({module}, {});

    EXPECT_FALSE(was_destroyed);
  }
  EXPECT_TRUE(was_destroyed);
}

// Regression test to validate that when modules are partitioned into modules to
// keep and modules to remove, the modules to remove are not destroyed.
// https://crbug.com/1127466 case 2.
MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesPartitioning) {
  int destroyed_count = 0;
  const auto record_destroyed = [&destroyed_count] { ++destroyed_count; };
  {
    ModuleCache cache;
    std::vector<std::unique_ptr<const ModuleCache::Module>> modules;
    modules.push_back(std::make_unique<FakeModule>(
        1, 1, false, BindLambdaForTesting(record_destroyed)));
    const ModuleCache::Module* module1 = modules.back().get();
    modules.push_back(std::make_unique<FakeModule>(
        2, 1, false, BindLambdaForTesting(record_destroyed)));
    cache.UpdateNonNativeModules({}, std::move(modules));
    cache.UpdateNonNativeModules({module1}, {});

    EXPECT_EQ(0, destroyed_count);
  }
  EXPECT_EQ(2, destroyed_count);
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesReplace) {
  ModuleCache cache;
  // Replace a module with another larger module at the same base address.
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules1;
  modules1.push_back(std::make_unique<FakeModule>(1, 1, false));
  const ModuleCache::Module* module1 = modules1.back().get();
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules2;
  modules2.push_back(std::make_unique<FakeModule>(1, 2, false));
  const ModuleCache::Module* module2 = modules2.back().get();

  cache.UpdateNonNativeModules({}, std::move(modules1));
  cache.UpdateNonNativeModules({module1}, std::move(modules2));

  EXPECT_EQ(module2, cache.GetModuleForAddress(2));
}

MAYBE_TEST(ModuleCacheTest,
           UpdateNonNativeModulesMultipleRemovedModulesAtSameAddress) {
  int destroyed_count = 0;
  const auto record_destroyed = [&destroyed_count] { ++destroyed_count; };
  ModuleCache cache;

  // Checks that non-native modules can be repeatedly added and removed at the
  // same addresses, and that all are retained in the cache.
  std::vector<std::unique_ptr<const ModuleCache::Module>> modules1;
  modules1.push_back(std::make_unique<FakeModule>(
      1, 1, false, BindLambdaForTesting(record_destroyed)));
  const ModuleCache::Module* module1 = modules1.back().get();

  std::vector<std::unique_ptr<const ModuleCache::Module>> modules2;
  modules2.push_back(std::make_unique<FakeModule>(
      1, 1, false, BindLambdaForTesting(record_destroyed)));
  const ModuleCache::Module* module2 = modules2.back().get();

  cache.UpdateNonNativeModules({}, std::move(modules1));
  cache.UpdateNonNativeModules({module1}, std::move(modules2));
  cache.UpdateNonNativeModules({module2}, {});

  EXPECT_EQ(0, destroyed_count);
}

MAYBE_TEST(ModuleCacheTest, UpdateNonNativeModulesCorrectModulesRemoved) {
  ModuleCache cache;

  std::vector<std::unique_ptr<const ModuleCache::Module>> to_add;
  for (int i = 0; i < 5; ++i) {
    to_add.push_back(std::make_unique<FakeModule>(i + 1, 1, false));
  }

  std::vector<const ModuleCache::Module*> to_remove = {to_add[1].get(),
                                                       to_add[3].get()};

  // Checks that the correct modules are removed when removing some but not all
  // modules.
  cache.UpdateNonNativeModules({}, std::move(to_add));
  cache.UpdateNonNativeModules({to_remove}, {});

  DCHECK_NE(nullptr, cache.GetModuleForAddress(1));
  DCHECK_EQ(nullptr, cache.GetModuleForAddress(2));
  DCHECK_NE(nullptr, cache.GetModuleForAddress(3));
  DCHECK_EQ(nullptr, cache.GetModuleForAddress(4));
  DCHECK_NE(nullptr, cache.GetModuleForAddress(5));
}

MAYBE_TEST(ModuleCacheTest, ModulesList) {
  ModuleCache cache;
  uintptr_t ptr = reinterpret_cast<uintptr_t>(&AFunctionForTest);
  const ModuleCache::Module* native_module = cache.GetModuleForAddress(ptr);
  const ModuleCache::Module* non_native_module =
      AddNonNativeModule(&cache, std::make_unique<FakeModule>(1, 2, false));

  EXPECT_NE(nullptr, native_module);
  std::vector<const ModuleCache::Module*> modules = cache.GetModules();
  ASSERT_EQ(2u, modules.size());
  EXPECT_EQ(native_module, modules[0]);
  EXPECT_EQ(non_native_module, modules[1]);
}

MAYBE_TEST(ModuleCacheTest, InvalidModule) {
  ModuleCache cache;
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));
}

// arm64 module support is not implemented.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    (BUILDFLAG(IS_ANDROID) && !defined(ARCH_CPU_ARM64))
// Validates that, for the memory regions listed in /proc/self/maps, the modules
// found via ModuleCache are consistent with those regions' extents.
TEST(ModuleCacheTest, CheckAgainstProcMaps) {
  std::string proc_maps;
  debug::ReadProcMaps(&proc_maps);
  std::vector<debug::MappedMemoryRegion> regions;
  ASSERT_TRUE(debug::ParseProcMaps(proc_maps, &regions));

  // Map distinct paths to lists of regions for the path in increasing memory
  // order.
  using RegionVector = std::vector<const debug::MappedMemoryRegion*>;
  using PathRegionsMap = std::map<std::string_view, RegionVector>;
  PathRegionsMap path_regions;
  for (const debug::MappedMemoryRegion& region : regions)
    path_regions[region.path].push_back(&region);

  const auto find_last_executable_region = [](const RegionVector& regions) {
    const auto rloc = base::ranges::find_if(
        base::Reversed(regions), [](const debug::MappedMemoryRegion* region) {
          return static_cast<bool>(region->permissions &
                                   debug::MappedMemoryRegion::EXECUTE);
        });
    return rloc == regions.rend() ? nullptr : *rloc;
  };

  int module_count = 0;

  // Loop through each distinct path.
  for (const auto& path_regions_pair : path_regions) {
    // Regions that aren't associated with absolute paths are unlikely to be
    // part of modules.
    if (path_regions_pair.first.empty() || path_regions_pair.first[0] != '/')
      continue;

    const debug::MappedMemoryRegion* const last_executable_region =
        find_last_executable_region(path_regions_pair.second);
    // The region isn't part of a module if no executable regions are associated
    // with the same path.
    if (!last_executable_region)
      continue;

    // Loop through all the regions associated with the path, checking that
    // modules created for addresses in each region have the expected extents.
    const uintptr_t expected_base_address =
        path_regions_pair.second.front()->start;
    for (const auto* region : path_regions_pair.second) {
      ModuleCache cache;
      const ModuleCache::Module* module =
          cache.GetModuleForAddress(region->start);
      // Not all regions matching the prior conditions are necessarily modules;
      // things like resources are also mmapped into memory from files. Ignore
      // any region isn't part of a module.
      if (!module)
        continue;

      ++module_count;

      EXPECT_EQ(expected_base_address, module->GetBaseAddress());
      // This needs an inequality comparison because the module size is computed
      // based on the ELF section's actual extent, while the |proc_maps| region
      // is aligned to a larger boundary.
      EXPECT_LE(module->GetSize(),
                last_executable_region->end - expected_base_address)
          << "base address: " << std::hex << module->GetBaseAddress()
          << std::endl
          << "region start: " << std::hex << region->start << std::endl
          << "region end: " << std::hex << region->end << std::endl;
    }
  }

  // Linux should have at least this module and ld-linux.so. Android should have
  // at least this module and system libraries.
  EXPECT_GE(module_count, 2);
}
#endif

// Module provider that always return a fake module of size 1 for a given
// |address|.
class MockModuleProvider : public ModuleCache::AuxiliaryModuleProvider {
 public:
  explicit MockModuleProvider(size_t module_size = 1)
      : module_size_(module_size) {}

  std::unique_ptr<const ModuleCache::Module> TryCreateModuleForAddress(
      uintptr_t address) override {
    return std::make_unique<FakeModule>(address, module_size_);
  }

 private:
  size_t module_size_;
};

// Check that auxiliary provider can inject new modules when registered.
TEST(ModuleCacheTest, RegisterAuxiliaryModuleProvider) {
  ModuleCache cache;
  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));

  MockModuleProvider auxiliary_provider;
  cache.RegisterAuxiliaryModuleProvider(&auxiliary_provider);
  auto* module = cache.GetModuleForAddress(1);
  EXPECT_NE(nullptr, module);
  EXPECT_EQ(1U, module->GetBaseAddress());
  cache.UnregisterAuxiliaryModuleProvider(&auxiliary_provider);

  // Even when unregistered, the module remains in the cache.
  EXPECT_EQ(module, cache.GetModuleForAddress(1));
}

// Check that ModuleCache's own module creator is used preferentially over
// auxiliary provider if possible.
MAYBE_TEST(ModuleCacheTest, NativeModuleOverAuxiliaryModuleProvider) {
  ModuleCache cache;

  MockModuleProvider auxiliary_provider(/*module_size=*/100);
  cache.RegisterAuxiliaryModuleProvider(&auxiliary_provider);

  const ModuleCache::Module* module =
      cache.GetModuleForAddress(reinterpret_cast<uintptr_t>(&AFunctionForTest));
  ASSERT_NE(nullptr, module);

  // The module should be a native module, which will have size greater than 100
  // bytes.
  EXPECT_NE(100u, module->GetSize());
  cache.UnregisterAuxiliaryModuleProvider(&auxiliary_provider);
}

// Check that auxiliary provider is no longer used after being unregistered.
TEST(ModuleCacheTest, UnregisterAuxiliaryModuleProvider) {
  ModuleCache cache;

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));

  MockModuleProvider auxiliary_provider;
  cache.RegisterAuxiliaryModuleProvider(&auxiliary_provider);
  cache.UnregisterAuxiliaryModuleProvider(&auxiliary_provider);

  EXPECT_EQ(nullptr, cache.GetModuleForAddress(1));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
TEST(ModuleCacheTest, TransformELFToSymbolServerFormat) {
  // See explanation for the module_id mangling in
  // base::TransformModuleIDToSymbolServerFormat implementation.
  EXPECT_EQ(TransformModuleIDToSymbolServerFormat(
                "7F0715C286F8B16C10E4AD349CDA3B9B56C7A773"),
            "C215077FF8866CB110E4AD349CDA3B9B0");
}
#endif

}  // namespace
}  // namespace base
