// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_RESTRICTIONS_H_
#define BASE_THREADING_THREAD_RESTRICTIONS_H_

#include <memory>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "build/build_config.h"

// -----------------------------------------------------------------------------
// Usage documentation
// -----------------------------------------------------------------------------
//
// Overview:
// This file exposes functions to ban and allow certain slow operations
// on a per-thread basis. To annotate *usage* of such slow operations, refer to
// scoped_blocking_call.h instead.
//
// Specific allowances that can be controlled in this file are:
// - Blocking call: Refers to any call that causes the calling thread to wait
//   off-CPU. It includes but is not limited to calls that wait on synchronous
//   file I/O operations: read or write a file from disk, interact with a pipe
//   or a socket, rename or delete a file, enumerate files in a directory, etc.
//   Acquiring a low contention lock is not considered a blocking call.
//
// - Waiting on a //base sync primitive: Refers to calling one of these methods:
//   - base::WaitableEvent::*Wait*
//   - base::ConditionVariable::*Wait*
//   - base::Process::WaitForExit*
//
// - Accessing singletons: Accessing global state (Singleton / LazyInstance) is
//   problematic on threads whom aren't joined on shutdown as they can be using
//   the state as it becomes invalid during tear down. base::NoDestructor is the
//   preferred alternative for global state and doesn't have this restriction.
//
// - Long CPU work: Refers to any code that takes more than 100 ms to
//   run when there is no CPU contention and no hard page faults and therefore,
//   is not suitable to run on a thread required to keep the browser responsive
//   (where jank could be visible to the user).
//
// The following disallowance functions are offered:
//  - DisallowBlocking(): Disallows blocking calls on the current thread.
//  - DisallowBaseSyncPrimitives(): Disallows waiting on a //base sync primitive
//    on the current thread.
//  - DisallowSingleton(): Disallows using singletons on the current thread.
//  - DisallowUnresponsiveTasks() Disallows blocking calls, waiting on a //base
//    sync primitive, and long CPU work on the current thread.
//
// In addition, scoped-allowance mechanisms are offered to make an exception
// within a scope for a behavior that is normally disallowed.
//  - ScopedAllowBlocking(ForTesting): Allows blocking calls.
//  - ScopedAllowBaseSyncPrimitives(ForTesting)(OutsideBlockingScope): Allow
//    waiting on a //base sync primitive. The OutsideBlockingScope suffix allows
//    uses in a scope where blocking is also disallowed.
//
// Avoid using allowances outside of unit tests. In unit tests, use allowances
// with the suffix "ForTesting".
//
// Prefer making blocking calls from tasks posted to base::ThreadPoolInstance
// with base::MayBlock().
//
// Instead of waiting on a WaitableEvent or a ConditionVariable, prefer putting
// the work that should happen after the wait in a continuation callback and
// post it from where the WaitableEvent or ConditionVariable would have been
// signaled. If something needs to be scheduled after many tasks have executed,
// use base::BarrierClosure.
//
// On Windows, join processes asynchronously using base::win::ObjectWatcher.
//
// Where unavoidable, put ScopedAllow* instances in the narrowest scope possible
// in the caller making the blocking call but no further down. For example: if a
// Cleanup() method needs to do a blocking call, document Cleanup() as blocking
// and add a ScopedAllowBlocking instance in callers that can't avoid making
// this call from a context where blocking is banned, as such:
//
//   void Client::MyMethod() {
//     (...)
//     {
//       // Blocking is okay here because XYZ.
//       ScopedAllowBlocking allow_blocking;
//       my_foo_->Cleanup();
//     }
//     (...)
//   }
//
//   // This method can block.
//   void Foo::Cleanup() {
//     // Do NOT add the ScopedAllowBlocking in Cleanup() directly as that hides
//     // its blocking nature from unknowing callers and defeats the purpose of
//     // these checks.
//     FlushStateToDisk();
//   }
//
// Note: In rare situations where the blocking call is an implementation detail
// (i.e. the impl makes a call that invokes AssertBlockingAllowed() but it
// somehow knows that in practice this will not block), it might be okay to hide
// the ScopedAllowBlocking instance in the impl with a comment explaining why
// that's okay.

class BrowserProcessImpl;
class ChromeNSSCryptoModuleDelegate;
class KeyStorageLinux;
class NativeBackendKWallet;
class NativeDesktopMediaList;
class Profile;
class StartupTabProviderImpl;
class GaiaConfig;
class WebEngineBrowserMainParts;

Profile* GetLastProfileMac();

namespace android_webview {
class AwFormDatabaseService;
class CookieManager;
class ScopedAllowInitGLBindings;
class VizCompositorThreadRunnerWebView;
}  // namespace android_webview
namespace ash {
class MojoUtils;
class BrowserDataMigrator;
bool CameraAppUIShouldEnableLocalOverride(const std::string&);
}  // namespace ash
namespace audio {
class OutputDevice;
}
namespace blink {
class CategorizedWorkerPoolImpl;
class CategorizedWorkerPoolJob;
class CategorizedWorkerPool;
class DiskDataAllocator;
class IdentifiabilityActiveSampler;
class RTCVideoDecoderAdapter;
class RTCVideoEncoder;
class SourceStream;
class VideoFrameResourceProvider;
class WebRtcVideoFrameAdapter;
class LegacyWebRtcVideoFrameAdapter;
class WorkerThread;
namespace scheduler {
class NonMainThreadImpl;
}
}  // namespace blink
namespace cc {
class CompletionEvent;
class TileTaskManagerImpl;
}  // namespace cc
namespace chromecast {
class CrashUtil;
}
namespace chromeos {
class BlockingMethodCaller;
namespace system {
class StatisticsProviderImpl;
bool IsCoreSchedulingAvailable();
int NumberOfPhysicalCores();
}  // namespace system
}  // namespace chromeos
namespace chrome_cleaner {
class ResetShortcutsComponent;
class SystemReportComponent;
}  // namespace chrome_cleaner
namespace content {
class BrowserGpuChannelHostFactory;
class BrowserMainLoop;
class BrowserProcessIOThread;
class BrowserTestBase;
class DesktopCaptureDevice;
class DWriteFontCollectionProxy;
class DWriteFontProxyImpl;
class EmergencyTraceFinalisationCoordinator;
class InProcessUtilityThread;
class NestedMessagePumpAndroid;
class NetworkServiceInstancePrivate;
class PepperPrintSettingsManagerImpl;
class RenderProcessHostImpl;
class RenderProcessHost;
class RenderWidgetHostViewMac;
class RendererBlinkPlatformImpl;
class RTCVideoDecoder;
class SandboxHostLinux;
class ScopedAllowWaitForDebugURL;
class ServiceWorkerContextClient;
class ShellPathProvider;
class SynchronousCompositor;
class SynchronousCompositorHost;
class SynchronousCompositorSyncCallBridge;
class TextInputClientMac;
class WebContentsImpl;
class WebContentsViewMac;
}  // namespace content
namespace cronet {
class CronetPrefsManager;
class CronetContext;
}  // namespace cronet
namespace crosapi {
class LacrosThreadTypeDelegate;
}  // namespace crosapi
namespace dbus {
class Bus;
}
namespace device {
class UsbContext;
}
namespace base {
class FilePath;
}
namespace disk_cache {
class BackendImpl;
class InFlightIO;
bool CleanupDirectorySync(const base::FilePath&);
}  // namespace disk_cache
namespace enterprise_connectors {
class LinuxKeyRotationCommand;
}  // namespace enterprise_connectors
namespace functions {
class ExecScriptScopedAllowBaseSyncPrimitives;
}
namespace history_report {
class HistoryReportJniBridge;
}
namespace ios_web_view {
class WebViewBrowserState;
}
namespace leveldb::port {
class ScopedAllowWait;
}  // namespace leveldb::port
namespace location::nearby::chrome {
class ScheduledExecutor;
class SubmittableExecutor;
}  // namespace location::nearby::chrome
namespace media {
class AudioInputDevice;
class AudioOutputDevice;
class BlockingUrlProtocol;
class FileVideoCaptureDeviceFactory;
class PaintCanvasVideoRenderer;
}  // namespace media
namespace memory_instrumentation {
class OSMetrics;
}
namespace metrics {
class AndroidMetricsServiceClient;
class CleanExitBeacon;
}  // namespace metrics
namespace midi {
class TaskService;  // https://crbug.com/796830
}
namespace module_installer {
class ScopedAllowModulePakLoad;
}
namespace mojo {
class CoreLibraryInitializer;
class SyncCallRestrictions;
namespace core {
class ScopedIPCSupport;
}
}  // namespace mojo
namespace printing {
class LocalPrinterHandlerDefault;
#if BUILDFLAG(IS_MAC)
class PrintBackendServiceImpl;
#endif
class PrintBackendServiceManager;
class PrintJobWorker;
class PrinterQuery;
}  // namespace printing
namespace rlz_lib {
class FinancialPing;
}
namespace storage {
class ObfuscatedFileUtil;
}
namespace syncer {
class GetLocalChangesRequest;
class HttpBridge;
}  // namespace syncer
namespace ui {
class DrmThreadProxy;
}
namespace value_store {
class LeveldbValueStore;
}
namespace weblayer {
class BrowserContextImpl;
class ContentBrowserClientImpl;
class ProfileImpl;
class WebLayerPathProvider;
}  // namespace weblayer
namespace net {
class MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives;
class MultiThreadedProxyResolverScopedAllowJoinOnIO;
class NetworkChangeNotifierMac;
class NetworkConfigWatcherMacThread;
namespace internal {
class AddressTrackerLinux;
}
}  // namespace net

namespace proxy_resolver {
class ScopedAllowThreadJoinForProxyResolverV8Tracing;
}

namespace remote_cocoa {
class DroppedScreenShotCopierMac;
}  // namespace remote_cocoa

namespace remoting {
class AutoThread;
class ScopedBypassIOThreadRestrictions;
namespace protocol {
class ScopedAllowSyncPrimitivesForWebRtcTransport;
class ScopedAllowThreadJoinForWebRtcTransport;
}  // namespace protocol
}  // namespace remoting

namespace service_manager {
class ServiceProcessLauncher;
}

namespace shell_integration_linux {
class LaunchXdgUtilityScopedAllowBaseSyncPrimitives;
}

namespace tracing {
class FuchsiaPerfettoProducerConnector;
}

namespace ui {
class WindowResizeHelperMac;
}

namespace viz {
class HostGpuMemoryBufferManager;
}

namespace vr {
class VrShell;
}

namespace web {
class WebMainLoop;
class WebSubThread;
}  // namespace web

namespace webrtc {
class DesktopConfigurationMonitor;
}

namespace base {
class Environment;
}

bool HasWaylandDisplay(base::Environment* env);

namespace base {

namespace sequence_manager::internal {
class TaskQueueImpl;
}  // namespace sequence_manager::internal

namespace android {
class JavaHandlerThread;
}

namespace internal {
class GetAppOutputScopedAllowBaseSyncPrimitives;
class JobTaskSource;
class TaskTracker;
}  // namespace internal

namespace win {
class OSInfo;
}

class AdjustOOMScoreHelper;
class FileDescriptorWatcher;
class FilePath;
class ScopedAllowThreadRecallForStackSamplingProfiler;
class StackSamplingProfiler;
class TestCustomDisallow;
class SimpleThread;
class Thread;

class BooleanWithStack;

bool PathProviderWin(int, FilePath*);

#if DCHECK_IS_ON()
// NOT_TAIL_CALLED if dcheck-is-on so it's always evident who irrevocably
// altered the allowance (dcheck-builds will provide the setter's stack on
// assertion) or who made a failing Assert*() call.
#define INLINE_OR_NOT_TAIL_CALLED BASE_EXPORT NOT_TAIL_CALLED
#define EMPTY_BODY_IF_DCHECK_IS_OFF
#else
// inline if dcheck-is-off so it's no overhead
#define INLINE_OR_NOT_TAIL_CALLED inline

// The static_assert() eats follow-on semicolons. `= default` would work
// too, but it makes clang realize that all the Scoped classes are no-ops in
// non-dcheck builds and it starts emitting many -Wunused-variable warnings.
#define EMPTY_BODY_IF_DCHECK_IS_OFF \
  {}                                \
  static_assert(true, "")
#endif  // DCHECK_IS_ON()

namespace internal {

// Asserts that blocking calls are allowed in the current scope. This is an
// internal call, external code should use ScopedBlockingCall instead, which
// serves as a precise annotation of the scope that may/will block.
INLINE_OR_NOT_TAIL_CALLED void AssertBlockingAllowed()
    EMPTY_BODY_IF_DCHECK_IS_OFF;
INLINE_OR_NOT_TAIL_CALLED void AssertBlockingDisallowedForTesting()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

}  // namespace internal

// Disallows blocking on the current thread.
INLINE_OR_NOT_TAIL_CALLED void DisallowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;

// Disallows blocking calls within its scope.
class BASE_EXPORT ScopedDisallowBlocking {
 public:
  ScopedDisallowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;

  ScopedDisallowBlocking(const ScopedDisallowBlocking&) = delete;
  ScopedDisallowBlocking& operator=(const ScopedDisallowBlocking&) = delete;

  ~ScopedDisallowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;

 private:
#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
};

class BASE_EXPORT ScopedAllowBlocking {
 public:
  ScopedAllowBlocking(const ScopedAllowBlocking&) = delete;
  ScopedAllowBlocking& operator=(const ScopedAllowBlocking&) = delete;

 private:
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest,
                           NestedAllowRestoresPreviousStack);
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest, ScopedAllowBlocking);
  friend class ScopedAllowBlockingForTesting;

  // This can only be instantiated by friends. Use ScopedAllowBlockingForTesting
  // in unit tests to avoid the friend requirement.
  friend class ::GaiaConfig;
  friend class ::StartupTabProviderImpl;
  friend class android_webview::ScopedAllowInitGLBindings;
  friend class ash::MojoUtils;  // http://crbug.com/1055467
  friend class ash::BrowserDataMigrator;
  friend class base::AdjustOOMScoreHelper;
  friend class base::StackSamplingProfiler;
  friend class blink::DiskDataAllocator;
  friend class chromecast::CrashUtil;
  friend class content::BrowserProcessIOThread;
  friend class content::DWriteFontProxyImpl;
  friend class content::NetworkServiceInstancePrivate;
  friend class content::PepperPrintSettingsManagerImpl;
  friend class content::RenderProcessHostImpl;
  friend class content::RenderWidgetHostViewMac;  // http://crbug.com/121917
  friend class content::ShellPathProvider;
#if BUILDFLAG(IS_WIN)
  friend class base::win::OSInfo;
  friend class content::WebContentsImpl;  // http://crbug.com/1262162
#endif
  friend class content::WebContentsViewMac;
  friend class cronet::CronetPrefsManager;
  friend class cronet::CronetContext;
  friend class crosapi::LacrosThreadTypeDelegate;
  friend class ios_web_view::WebViewBrowserState;
  friend class media::FileVideoCaptureDeviceFactory;
  friend class memory_instrumentation::OSMetrics;
  friend class metrics::AndroidMetricsServiceClient;
  friend class metrics::CleanExitBeacon;
  friend class module_installer::ScopedAllowModulePakLoad;
  friend class mojo::CoreLibraryInitializer;
  friend class printing::LocalPrinterHandlerDefault;
#if BUILDFLAG(IS_MAC)
  friend class printing::PrintBackendServiceImpl;
#endif
  friend class printing::PrintBackendServiceManager;
  friend class printing::PrintJobWorker;
  friend class remote_cocoa::
      DroppedScreenShotCopierMac;  // https://crbug.com/1148078
  friend class remoting::ScopedBypassIOThreadRestrictions;  // crbug.com/1144161
  friend class web::WebSubThread;
  friend class ::WebEngineBrowserMainParts;
  friend class weblayer::BrowserContextImpl;
  friend class weblayer::ContentBrowserClientImpl;
  friend class weblayer::ProfileImpl;
  friend class weblayer::WebLayerPathProvider;

  // Sorting with function name (with namespace), ignoring the return type.
  friend Profile* ::GetLastProfileMac();  // crbug.com/1176734
  friend bool ::HasWaylandDisplay(base::Environment* env);  // crbug.com/1246928
  friend bool PathProviderWin(int, FilePath*);
  friend bool ash::CameraAppUIShouldEnableLocalOverride(const std::string&);
  friend bool chromeos::system::IsCoreSchedulingAvailable();
  friend int chromeos::system::NumberOfPhysicalCores();
  friend bool disk_cache::CleanupDirectorySync(const base::FilePath&);

  ScopedAllowBlocking(const Location& from_here = Location::Current());
  ~ScopedAllowBlocking();

#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
};

class ScopedAllowBlockingForTesting {
 public:
  ScopedAllowBlockingForTesting() {}

  ScopedAllowBlockingForTesting(const ScopedAllowBlockingForTesting&) = delete;
  ScopedAllowBlockingForTesting& operator=(
      const ScopedAllowBlockingForTesting&) = delete;

  ~ScopedAllowBlockingForTesting() {}

 private:
#if DCHECK_IS_ON()
  ScopedAllowBlocking scoped_allow_blocking_;
#endif
};

INLINE_OR_NOT_TAIL_CALLED void DisallowBaseSyncPrimitives()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

// Disallows singletons within its scope.
class BASE_EXPORT ScopedDisallowBaseSyncPrimitives {
 public:
  ScopedDisallowBaseSyncPrimitives() EMPTY_BODY_IF_DCHECK_IS_OFF;

  ScopedDisallowBaseSyncPrimitives(const ScopedDisallowBaseSyncPrimitives&) =
      delete;
  ScopedDisallowBaseSyncPrimitives& operator=(
      const ScopedDisallowBaseSyncPrimitives&) = delete;

  ~ScopedDisallowBaseSyncPrimitives() EMPTY_BODY_IF_DCHECK_IS_OFF;

 private:
#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
};

class BASE_EXPORT ScopedAllowBaseSyncPrimitives {
 public:
  ScopedAllowBaseSyncPrimitives(const ScopedAllowBaseSyncPrimitives&) = delete;
  ScopedAllowBaseSyncPrimitives& operator=(
      const ScopedAllowBaseSyncPrimitives&) = delete;

 private:
  // This can only be instantiated by friends. Use
  // ScopedAllowBaseSyncPrimitivesForTesting in unit tests to avoid the friend
  // requirement.
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest,
                           ScopedAllowBaseSyncPrimitives);
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest,
                           ScopedAllowBaseSyncPrimitivesResetsState);
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest,
                           ScopedAllowBaseSyncPrimitivesWithBlockingDisallowed);

  // Allowed usage:
  friend class ::ChromeNSSCryptoModuleDelegate;
  friend class base::internal::GetAppOutputScopedAllowBaseSyncPrimitives;
  friend class base::SimpleThread;
  friend class blink::CategorizedWorkerPoolImpl;
  friend class blink::CategorizedWorkerPoolJob;
  friend class blink::IdentifiabilityActiveSampler;
  friend class blink::SourceStream;
  friend class blink::WorkerThread;
  friend class blink::scheduler::NonMainThreadImpl;
  friend class chrome_cleaner::ResetShortcutsComponent;
  friend class chrome_cleaner::SystemReportComponent;
  friend class content::BrowserMainLoop;
  friend class content::BrowserProcessIOThread;
  friend class content::RendererBlinkPlatformImpl;
  friend class content::DWriteFontCollectionProxy;
  friend class content::ServiceWorkerContextClient;
  friend class device::UsbContext;
  friend class enterprise_connectors::LinuxKeyRotationCommand;
  friend class functions::ExecScriptScopedAllowBaseSyncPrimitives;
  friend class history_report::HistoryReportJniBridge;
  friend class internal::TaskTracker;
  friend class leveldb::port::ScopedAllowWait;
  friend class location::nearby::chrome::ScheduledExecutor;
  friend class location::nearby::chrome::SubmittableExecutor;
  friend class media::BlockingUrlProtocol;
  friend class mojo::core::ScopedIPCSupport;
  friend class net::MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives;
  friend class rlz_lib::FinancialPing;
  friend class shell_integration_linux::
      LaunchXdgUtilityScopedAllowBaseSyncPrimitives;
  friend class storage::ObfuscatedFileUtil;
  friend class syncer::HttpBridge;
  friend class syncer::GetLocalChangesRequest;
  friend class webrtc::DesktopConfigurationMonitor;
  friend class ::tracing::FuchsiaPerfettoProducerConnector;

  // Usage that should be fixed:
  friend class ::NativeBackendKWallet;  // http://crbug.com/125331
  friend class ::chromeos::system::
      StatisticsProviderImpl;                      // http://crbug.com/125385
  friend class blink::VideoFrameResourceProvider;  // http://crbug.com/878070
  friend class value_store::LeveldbValueStore;     // http://crbug.com/1330845

  ScopedAllowBaseSyncPrimitives() EMPTY_BODY_IF_DCHECK_IS_OFF;
  ~ScopedAllowBaseSyncPrimitives() EMPTY_BODY_IF_DCHECK_IS_OFF;

#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
};

class BASE_EXPORT ScopedAllowBaseSyncPrimitivesOutsideBlockingScope {
 public:
  ScopedAllowBaseSyncPrimitivesOutsideBlockingScope(
      const ScopedAllowBaseSyncPrimitivesOutsideBlockingScope&) = delete;
  ScopedAllowBaseSyncPrimitivesOutsideBlockingScope& operator=(
      const ScopedAllowBaseSyncPrimitivesOutsideBlockingScope&) = delete;

 private:
  // This can only be instantiated by friends. Use
  // ScopedAllowBaseSyncPrimitivesForTesting in unit tests to avoid the friend
  // requirement.
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest,
                           ScopedAllowBaseSyncPrimitivesOutsideBlockingScope);
  FRIEND_TEST_ALL_PREFIXES(
      ThreadRestrictionsTest,
      ScopedAllowBaseSyncPrimitivesOutsideBlockingScopeResetsState);

  // Allowed usage:
  friend class ::BrowserProcessImpl;  // http://crbug.com/125207
  friend class ::KeyStorageLinux;
  friend class ::NativeDesktopMediaList;
  friend class android::JavaHandlerThread;
  friend class android_webview::
      AwFormDatabaseService;  // http://crbug.com/904431
  friend class android_webview::CookieManager;
  friend class android_webview::VizCompositorThreadRunnerWebView;
  friend class audio::OutputDevice;
  friend class base::sequence_manager::internal::TaskQueueImpl;
  friend class base::FileDescriptorWatcher;
  friend class base::internal::JobTaskSource;
  friend class base::ScopedAllowThreadRecallForStackSamplingProfiler;
  friend class base::StackSamplingProfiler;
  friend class blink::CategorizedWorkerPoolImpl;
  friend class blink::CategorizedWorkerPoolJob;
  friend class blink::CategorizedWorkerPool;
  friend class blink::RTCVideoDecoderAdapter;
  friend class blink::RTCVideoEncoder;
  friend class blink::WebRtcVideoFrameAdapter;
  friend class blink::LegacyWebRtcVideoFrameAdapter;
  friend class cc::TileTaskManagerImpl;
  friend class content::DesktopCaptureDevice;
  friend class content::EmergencyTraceFinalisationCoordinator;
  friend class content::InProcessUtilityThread;
  friend class content::RTCVideoDecoder;
  friend class content::SandboxHostLinux;
  friend class content::ScopedAllowWaitForDebugURL;
  friend class content::SynchronousCompositor;
  friend class content::SynchronousCompositorHost;
  friend class content::SynchronousCompositorSyncCallBridge;
  friend class content::RenderProcessHost;
  friend class media::AudioInputDevice;
  friend class media::AudioOutputDevice;
  friend class media::PaintCanvasVideoRenderer;
  friend class mojo::SyncCallRestrictions;
  friend class net::NetworkConfigWatcherMacThread;
  friend class ui::DrmThreadProxy;
  friend class viz::HostGpuMemoryBufferManager;
  friend class vr::VrShell;

  // Usage that should be fixed:
  friend class ::chromeos::BlockingMethodCaller;  // http://crbug.com/125360
  friend class base::Thread;                      // http://crbug.com/918039
  friend class cc::CompletionEvent;               // http://crbug.com/902653
  friend class content::
      BrowserGpuChannelHostFactory;                 // http://crbug.com/125248
  friend class dbus::Bus;                           // http://crbug.com/125222
  friend class disk_cache::BackendImpl;             // http://crbug.com/74623
  friend class disk_cache::InFlightIO;              // http://crbug.com/74623
  friend class midi::TaskService;                   // https://crbug.com/796830
  friend class net::internal::AddressTrackerLinux;  // http://crbug.com/125097
  friend class net::
      MultiThreadedProxyResolverScopedAllowJoinOnIO;  // http://crbug.com/69710
  friend class net::NetworkChangeNotifierMac;         // http://crbug.com/125097
  friend class printing::PrinterQuery;                // http://crbug.com/66082
  friend class proxy_resolver::
      ScopedAllowThreadJoinForProxyResolverV8Tracing;  // http://crbug.com/69710
  friend class remoting::AutoThread;  // https://crbug.com/944316
  friend class remoting::protocol::
      ScopedAllowSyncPrimitivesForWebRtcTransport;  // http://crbug.com/1198501
  friend class remoting::protocol::
      ScopedAllowThreadJoinForWebRtcTransport;  // http://crbug.com/660081
  // Not used in production yet, https://crbug.com/844078.
  friend class service_manager::ServiceProcessLauncher;
  friend class ui::WindowResizeHelperMac;    // http://crbug.com/902829
  friend class content::TextInputClientMac;  // http://crbug.com/121917

  ScopedAllowBaseSyncPrimitivesOutsideBlockingScope(
      const Location& from_here = Location::Current());

  ~ScopedAllowBaseSyncPrimitivesOutsideBlockingScope();

#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
};

// Allow base-sync-primitives in tests, doesn't require explicit friend'ing like
// ScopedAllowBaseSyncPrimitives-types aimed at production do.
// Note: For WaitableEvents in the test logic, base::TestWaitableEvent is
// exposed as a convenience to avoid the need for
// ScopedAllowBaseSyncPrimitivesForTesting.
class BASE_EXPORT ScopedAllowBaseSyncPrimitivesForTesting {
 public:
  ScopedAllowBaseSyncPrimitivesForTesting() EMPTY_BODY_IF_DCHECK_IS_OFF;

  ScopedAllowBaseSyncPrimitivesForTesting(
      const ScopedAllowBaseSyncPrimitivesForTesting&) = delete;
  ScopedAllowBaseSyncPrimitivesForTesting& operator=(
      const ScopedAllowBaseSyncPrimitivesForTesting&) = delete;

  ~ScopedAllowBaseSyncPrimitivesForTesting() EMPTY_BODY_IF_DCHECK_IS_OFF;

 private:
#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
};

// Counterpart to base::DisallowUnresponsiveTasks() for tests to allow them to
// block their thread after it was banned.
class BASE_EXPORT ScopedAllowUnresponsiveTasksForTesting {
 public:
  ScopedAllowUnresponsiveTasksForTesting() EMPTY_BODY_IF_DCHECK_IS_OFF;

  ScopedAllowUnresponsiveTasksForTesting(
      const ScopedAllowUnresponsiveTasksForTesting&) = delete;
  ScopedAllowUnresponsiveTasksForTesting& operator=(
      const ScopedAllowUnresponsiveTasksForTesting&) = delete;

  ~ScopedAllowUnresponsiveTasksForTesting() EMPTY_BODY_IF_DCHECK_IS_OFF;

 private:
#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_base_sync_;
  std::unique_ptr<BooleanWithStack> was_disallowed_blocking_;
  std::unique_ptr<BooleanWithStack> was_disallowed_cpu_;
#endif
};

namespace internal {

// Asserts that waiting on a //base sync primitive is allowed in the current
// scope.
INLINE_OR_NOT_TAIL_CALLED void AssertBaseSyncPrimitivesAllowed()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

// Resets all thread restrictions on the current thread.
INLINE_OR_NOT_TAIL_CALLED void ResetThreadRestrictionsForTesting()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

// Check whether the current thread is allowed to use singletons (Singleton /
// LazyInstance).  DCHECKs if not.
INLINE_OR_NOT_TAIL_CALLED void AssertSingletonAllowed()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

}  // namespace internal

// Disallow using singleton on the current thread.
INLINE_OR_NOT_TAIL_CALLED void DisallowSingleton() EMPTY_BODY_IF_DCHECK_IS_OFF;

// Disallows singletons within its scope.
class BASE_EXPORT ScopedDisallowSingleton {
 public:
  ScopedDisallowSingleton() EMPTY_BODY_IF_DCHECK_IS_OFF;

  ScopedDisallowSingleton(const ScopedDisallowSingleton&) = delete;
  ScopedDisallowSingleton& operator=(const ScopedDisallowSingleton&) = delete;

  ~ScopedDisallowSingleton() EMPTY_BODY_IF_DCHECK_IS_OFF;

 private:
#if DCHECK_IS_ON()
  std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
};

// Asserts that running long CPU work is allowed in the current scope.
INLINE_OR_NOT_TAIL_CALLED void AssertLongCPUWorkAllowed()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

INLINE_OR_NOT_TAIL_CALLED void DisallowUnresponsiveTasks()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

class BASE_EXPORT ThreadRestrictions {
 public:
  ThreadRestrictions() = delete;

  // Constructing a ScopedAllowIO temporarily allows IO for the current
  // thread.  Doing this is almost certainly always incorrect.
  //
  // DEPRECATED. Use ScopedAllowBlocking(ForTesting).
  // TODO(crbug.com/766678): Migrate remaining users.
  class BASE_EXPORT ScopedAllowIO {
   public:
    ScopedAllowIO(const Location& from_here = Location::Current());

    ScopedAllowIO(const ScopedAllowIO&) = delete;
    ScopedAllowIO& operator=(const ScopedAllowIO&) = delete;

    ~ScopedAllowIO();

   private:
#if DCHECK_IS_ON()
    std::unique_ptr<BooleanWithStack> was_disallowed_;
#endif
  };
};

// Friend-only methods to permanently allow the current thread to use
// blocking/sync-primitives calls. Threads start out in the *allowed* state but
// are typically *disallowed* via the above base::Disallow*() methods after
// being initialized.
//
// Only use these to permanently set the allowance on a thread, e.g. on
// shutdown. For temporary allowances, use scopers above.
class BASE_EXPORT PermanentThreadAllowance {
 public:
  // Class is merely a namespace-with-friends.
  PermanentThreadAllowance() = delete;

 private:
  friend class base::TestCustomDisallow;
  friend class content::BrowserMainLoop;
  friend class content::BrowserTestBase;
  friend class web::WebMainLoop;

  static void AllowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;
  static void AllowBaseSyncPrimitives() EMPTY_BODY_IF_DCHECK_IS_OFF;
};

// Similar to PermanentThreadAllowance but separate because it's dangerous and
// should have even fewer friends.
class BASE_EXPORT PermanentSingletonAllowance {
 public:
  // Class is merely a namespace-with-friends.
  PermanentSingletonAllowance() = delete;

 private:
  // Re-allow singletons on this thread. Since //base APIs DisallowSingleton()
  // when they risk running past shutdown, this should only be called in rare
  // cases where the caller knows the process will be killed rather than
  // shutdown.
  static void AllowSingleton() EMPTY_BODY_IF_DCHECK_IS_OFF;
};

#undef INLINE_OR_NOT_TAIL_CALLED
#undef EMPTY_BODY_IF_DCHECK_IS_OFF

}  // namespace base

#endif  // BASE_THREADING_THREAD_RESTRICTIONS_H_
