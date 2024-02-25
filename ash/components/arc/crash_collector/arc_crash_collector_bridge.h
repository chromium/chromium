// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
#define ASH_COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_

#include <string>

#include "ash/components/arc/mojom/crash_collector.mojom.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/mojom/base/time.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Relays dumps for non-native ARC crashes to the crash reporter in Chrome OS.
class ArcCrashCollectorBridge : public KeyedService,
                                public mojom::CrashCollectorHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcCrashCollectorBridge* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcCrashCollectorBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcCrashCollectorBridge(content::BrowserContext* context,
                          ArcBridgeService* bridge);

  ArcCrashCollectorBridge(const ArcCrashCollectorBridge&) = delete;
  ArcCrashCollectorBridge& operator=(const ArcCrashCollectorBridge&) = delete;

  ~ArcCrashCollectorBridge() override;

  // mojom::CrashCollectorHost overrides.
  void DumpCrash(const std::string& type,
                 mojo::ScopedHandle pipe,
                 std::optional<base::TimeDelta> uptime) override;
  void DumpNativeCrash(const std::string& exec_name,
                       int32_t pid,
                       int64_t timestamp,
                       mojo::ScopedHandle minidump_fd) override;
  void DumpKernelCrash(mojo::ScopedHandle ramoops_handle) override;
  void SetBuildProperties(
      const std::string& device,
      const std::string& board,
      const std::string& cpu_abi,
      const std::optional<std::string>& fingerprint) override;

  static void EnsureFactoryBuilt();

 private:
  std::vector<std::string> CreateCrashReporterArgs();

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  std::string device_;
  std::string board_;
  std::string cpu_abi_;
  std::optional<std::string> fingerprint_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
