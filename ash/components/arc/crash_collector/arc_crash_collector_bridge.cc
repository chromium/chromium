// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/crash_collector/arc_crash_collector_bridge.h"

#include <inttypes.h>
#include <sysexits.h>
#include <unistd.h>

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace {

const char kCrashReporterPath[] = "/sbin/crash_reporter";

bool RunCrashReporter(const std::vector<std::string>& args, int stdin_fd) {
  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(stdin_fd, STDIN_FILENO);

  auto process = base::LaunchProcess(args, options);

  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    LOG(ERROR) << "Failed to wait for " << kCrashReporterPath;
    return false;
  }
  if (exit_code != EX_OK) {
    LOG(ERROR) << kCrashReporterPath << " failed with exit code " << exit_code;
    return false;
  }
  return true;
}

// Runs crash_reporter to save the java crash info provided via the pipe.
void RunJavaCrashReporter(const std::string& crash_type,
                          base::ScopedFD pipe,
                          std::vector<std::string> args,
                          std::optional<base::TimeDelta> uptime) {
  args.push_back("--arc_java_crash=" + crash_type);
  if (uptime) {
    args.push_back(
        base::StringPrintf("--arc_uptime=%" PRId64, uptime->InMilliseconds()));
  }

  if (!RunCrashReporter(args, pipe.get()))
    LOG(ERROR) << "Failed to run crash_reporter";
}

// Runs crash_reporter to save the native crash info provided via the files.
void RunNativeCrashReporter(const std::string& exec_name,
                            int32_t pid,
                            int64_t timestamp,
                            base::ScopedFD minidump_fd,
                            std::vector<std::string> args) {
  args.insert(args.end(),
              {"--exe=" + exec_name, base::StringPrintf("--pid=%d", pid),
               base::StringPrintf("--arc_native_time=%" PRId64, timestamp),
               "--arc_native"});

  if (!RunCrashReporter(args, minidump_fd.get()))
    LOG(ERROR) << "Failed to run crash_reporter";
}

// Runs crash_reporter to save the kernel crash info.
void RunKernelCrashReporter(base::ScopedFD ramoops_fd,
                            std::vector<std::string> args) {
  args.push_back("--arc_kernel");

  if (!RunCrashReporter(args, ramoops_fd.get()))
    LOG(ERROR) << "Failed to run crash_reporter";
}

}  // namespace

namespace arc {
namespace {

// Singleton factory for ArcCrashCollectorBridge.
class ArcCrashCollectorBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcCrashCollectorBridge,
          ArcCrashCollectorBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcCrashCollectorBridgeFactory";

  static ArcCrashCollectorBridgeFactory* GetInstance() {
    return base::Singleton<ArcCrashCollectorBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcCrashCollectorBridgeFactory>;
  ArcCrashCollectorBridgeFactory() = default;
  ~ArcCrashCollectorBridgeFactory() override = default;
};

}  // namespace

// static
ArcCrashCollectorBridge* ArcCrashCollectorBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcCrashCollectorBridgeFactory::GetForBrowserContext(context);
}

// static
ArcCrashCollectorBridge*
ArcCrashCollectorBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcCrashCollectorBridgeFactory::GetForBrowserContextForTesting(
      context);
}

ArcCrashCollectorBridge::ArcCrashCollectorBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->crash_collector()->SetHost(this);
}

ArcCrashCollectorBridge::~ArcCrashCollectorBridge() {
  arc_bridge_service_->crash_collector()->SetHost(nullptr);
}

void ArcCrashCollectorBridge::DumpCrash(const std::string& type,
                                        mojo::ScopedHandle pipe,
                                        std::optional<base::TimeDelta> uptime) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&RunJavaCrashReporter, type,
                     mojo::UnwrapPlatformHandle(std::move(pipe)).TakeFD(),
                     CreateCrashReporterArgs(), uptime));
}

void ArcCrashCollectorBridge::DumpNativeCrash(const std::string& exec_name,
                                              int32_t pid,
                                              int64_t timestamp,
                                              mojo::ScopedHandle minidump_fd) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(
          &RunNativeCrashReporter, exec_name, pid, timestamp,
          mojo::UnwrapPlatformHandle(std::move(minidump_fd)).TakeFD(),
          CreateCrashReporterArgs()));
}

void ArcCrashCollectorBridge::DumpKernelCrash(
    mojo::ScopedHandle ramoops_handle) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(
          &RunKernelCrashReporter,
          mojo::UnwrapPlatformHandle(std::move(ramoops_handle)).TakeFD(),
          CreateCrashReporterArgs()));
}

void ArcCrashCollectorBridge::SetBuildProperties(
    const std::string& device,
    const std::string& board,
    const std::string& cpu_abi,
    const std::optional<std::string>& fingerprint) {
  device_ = device;
  board_ = board;
  cpu_abi_ = cpu_abi;
  fingerprint_ = fingerprint;
}

std::vector<std::string> ArcCrashCollectorBridge::CreateCrashReporterArgs() {
  std::vector<std::string> args = {
      kCrashReporterPath,
      "--arc_device=" + device_,
      "--arc_board=" + board_,
      "--arc_cpu_abi=" + cpu_abi_,
  };
  if (fingerprint_)
    args.push_back("--arc_fingerprint=" + fingerprint_.value());

  return args;
}

// static
void ArcCrashCollectorBridge::EnsureFactoryBuilt() {
  ArcCrashCollectorBridgeFactory::GetInstance();
}

}  // namespace arc
