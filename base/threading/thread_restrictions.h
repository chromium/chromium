// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREAD_RESTRICTIONS_H_
#define BASE_THREADING_THREAD_RESTRICTIONS_H_

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"

class BrowserProcessImpl;
class HistogramSynchronizer;
class NativeBackendKWallet;
class KeyStorageLinux;
class AdblockIsolationProviderCompositor;

namespace android_webview {
class AwFormDatabaseService;
class CookieManager;
class ScopedAllowInitGLBindings;
}

namespace cc {
class CompletionEvent;
class SingleThreadTaskGraphRunner;
}
namespace chromeos {
class BlockingMethodCaller;
namespace system {
class StatisticsProviderImpl;
}
}
namespace chrome_browser_net {
class Predictor;
}
namespace content {
class BrowserGpuChannelHostFactory;
class BrowserGpuMemoryBufferManager;
class BrowserMainLoop;
class BrowserProcessSubThread;
class BrowserShutdownProfileDumper;
class BrowserSurfaceViewManager;
class BrowserTestBase;
class CategorizedWorkerPool;
class NestedMessagePumpAndroid;
class ScopedAllowWaitForAndroidLayoutTests;
class ScopedAllowWaitForDebugURL;
class SessionStorageDatabase;
class SoftwareOutputDeviceMus;
class SynchronousCompositor;
class SynchronousCompositorHost;
class SynchronousCompositorSyncCallBridge;
class TextInputClientMac;
}  // namespace content
namespace cronet {
class CronetPrefsManager;
class CronetURLRequestContext;
}  // namespace cronet
namespace dbus {
class Bus;
}
namespace disk_cache {
class BackendImpl;
class InFlightIO;
}
namespace functions {
class ExecScriptScopedAllowBaseSyncPrimitives;
}
namespace gpu {
class GpuChannelHost;
}
namespace leveldb {
class LevelDBMojoProxy;
}
namespace media {
class AudioInputDevice;
class BlockingUrlProtocol;
}
namespace midi {
class TaskService;  // https://crbug.com/796830
}
namespace mojo {
class CoreLibraryInitializer;
class SyncCallRestrictions;
namespace edk {
class ScopedIPCSupport;
}
}
namespace rlz_lib {
class FinancialPing;
}
namespace ui {
class CommandBufferClientImpl;
class CommandBufferLocal;
class GpuState;
class MaterialDesignController;
}
namespace net {
class MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives;
class NetworkChangeNotifierMac;
namespace internal {
class AddressTrackerLinux;
}
}

namespace remoting {
class AutoThread;
}

namespace resource_coordinator {
class TabManagerDelegate;
}

namespace service_manager {
class ServiceProcessLauncher;
}

namespace shell_integration {
class LaunchXdgUtilityScopedAllowBaseSyncPrimitives;
}

namespace ui {
class WindowResizeHelperMac;
}

namespace views {
class ScreenMus;
}

namespace viz {
class ServerGpuMemoryBufferManager;
}

namespace webrtc {
class DesktopConfigurationMonitor;
}

namespace base {

namespace android {
class JavaHandlerThread;
}

namespace internal {
class TaskTracker;
}

class GetAppOutputScopedAllowBaseSyncPrimitives;
class SimpleThread;
class StackSamplingProfiler;
class Thread;
class ThreadTestHelper;

#if DCHECK_IS_ON()
#define INLINE_IF_DCHECK_IS_OFF BASE_EXPORT
#define EMPTY_BODY_IF_DCHECK_IS_OFF
#else
#define INLINE_IF_DCHECK_IS_OFF inline
#define EMPTY_BODY_IF_DCHECK_IS_OFF \
  {}
#endif

// A "blocking call" refers to any call that causes the calling thread to wait
// off-CPU. It includes but is not limited to calls that wait on synchronous
// file I/O operations: read or write a file from disk, interact with a pipe or
// a socket, rename or delete a file, enumerate files in a directory, etc.
// Acquiring a low contention lock is not considered a blocking call.

// Asserts that blocking calls are allowed in the current scope.
//
// Style tip: It's best if you put AssertBlockingAllowed() checks as close to
// the blocking call as possible. For example:
//
// void ReadFile() {
//   PreWork();
//
//   base::AssertBlockingAllowed();
//   fopen(...);
//
//   PostWork();
// }
//
// void Bar() {
//   ReadFile();
// }
//
// void Foo() {
//   Bar();
// }
INLINE_IF_DCHECK_IS_OFF void AssertBlockingAllowed()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

// Disallows blocking on the current thread.
INLINE_IF_DCHECK_IS_OFF void DisallowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;

// Disallows blocking calls within its scope.
class BASE_EXPORT ScopedDisallowBlocking {
 public:
  ScopedDisallowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;
  ~ScopedDisallowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;

 private:
#if DCHECK_IS_ON()
  const bool was_disallowed_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedDisallowBlocking);
};

// ScopedAllowBlocking(ForTesting) allow blocking calls within a scope where
// they are normally disallowed.
//
// Avoid using this. Prefer making blocking calls from tasks posted to
// base::TaskScheduler with base::MayBlock().
class BASE_EXPORT ScopedAllowBlocking {
 private:
  // This can only be instantiated by friends. Use ScopedAllowBlockingForTesting
  // in unit tests to avoid the friend requirement.
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest, ScopedAllowBlocking);
  friend class android_webview::ScopedAllowInitGLBindings;
  friend class content::BrowserProcessSubThread;
  friend class cronet::CronetPrefsManager;
  friend class cronet::CronetURLRequestContext;
  friend class media::AudioInputDevice;
  friend class mojo::CoreLibraryInitializer;
  friend class resource_coordinator::TabManagerDelegate;  // crbug.com/778703
  friend class ui::MaterialDesignController;
  friend class ScopedAllowBlockingForTesting;
  friend class StackSamplingProfiler;

  ScopedAllowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;
  ~ScopedAllowBlocking() EMPTY_BODY_IF_DCHECK_IS_OFF;

#if DCHECK_IS_ON()
  const bool was_disallowed_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowBlocking);
};

class ScopedAllowBlockingForTesting {
 public:
  ScopedAllowBlockingForTesting() {}
  ~ScopedAllowBlockingForTesting() {}

 private:
#if DCHECK_IS_ON()
  ScopedAllowBlocking scoped_allow_blocking_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowBlockingForTesting);
};

// "Waiting on a //base sync primitive" refers to calling one of these methods:
// - base::WaitableEvent::*Wait*
// - base::ConditionVariable::*Wait*
// - base::Process::WaitForExit*

// Disallows waiting on a //base sync primitive on the current thread.
INLINE_IF_DCHECK_IS_OFF void DisallowBaseSyncPrimitives()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

// ScopedAllowBaseSyncPrimitives(ForTesting)(OutsideBlockingScope) allow waiting
// on a //base sync primitive within a scope where this is normally disallowed.
//
// Avoid using this.
//
// Instead of waiting on a WaitableEvent or a ConditionVariable, put the work
// that should happen after the wait in a callback and post that callback from
// where the WaitableEvent or ConditionVariable would have been signaled. If
// something needs to be scheduled after many tasks have executed, use
// base::BarrierClosure.
//
// On Windows, join processes asynchronously using base::win::ObjectWatcher.

// This can only be used in a scope where blocking is allowed.
class BASE_EXPORT ScopedAllowBaseSyncPrimitives {
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
  friend class base::GetAppOutputScopedAllowBaseSyncPrimitives;
  friend class content::BrowserProcessSubThread;
  friend class content::SessionStorageDatabase;
  friend class functions::ExecScriptScopedAllowBaseSyncPrimitives;
  friend class leveldb::LevelDBMojoProxy;
  friend class media::BlockingUrlProtocol;
  friend class net::MultiThreadedCertVerifierScopedAllowBaseSyncPrimitives;
  friend class rlz_lib::FinancialPing;
  friend class shell_integration::LaunchXdgUtilityScopedAllowBaseSyncPrimitives;
  friend class webrtc::DesktopConfigurationMonitor;

  ScopedAllowBaseSyncPrimitives() EMPTY_BODY_IF_DCHECK_IS_OFF;
  ~ScopedAllowBaseSyncPrimitives() EMPTY_BODY_IF_DCHECK_IS_OFF;

#if DCHECK_IS_ON()
  const bool was_disallowed_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowBaseSyncPrimitives);
};

// This can be used in a scope where blocking is disallowed.
class BASE_EXPORT ScopedAllowBaseSyncPrimitivesOutsideBlockingScope {
 private:
  // This can only be instantiated by friends. Use
  // ScopedAllowBaseSyncPrimitivesForTesting in unit tests to avoid the friend
  // requirement.
  FRIEND_TEST_ALL_PREFIXES(ThreadRestrictionsTest,
                           ScopedAllowBaseSyncPrimitivesOutsideBlockingScope);
  FRIEND_TEST_ALL_PREFIXES(
      ThreadRestrictionsTest,
      ScopedAllowBaseSyncPrimitivesOutsideBlockingScopeResetsState);
  friend class ::KeyStorageLinux;
  friend class content::SynchronousCompositor;
  friend class content::SynchronousCompositorHost;
  friend class content::SynchronousCompositorSyncCallBridge;
  friend class midi::TaskService;  // https://crbug.com/796830
  // Not used in production yet, https://crbug.com/844078.
  friend class service_manager::ServiceProcessLauncher;
  friend class ::AdblockIsolationProviderCompositor;

  ScopedAllowBaseSyncPrimitivesOutsideBlockingScope()
      EMPTY_BODY_IF_DCHECK_IS_OFF;
  ~ScopedAllowBaseSyncPrimitivesOutsideBlockingScope()
      EMPTY_BODY_IF_DCHECK_IS_OFF;

#if DCHECK_IS_ON()
  const bool was_disallowed_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowBaseSyncPrimitivesOutsideBlockingScope);
};

// This can be used in tests without being a friend of
// ScopedAllowBaseSyncPrimitives(OutsideBlockingScope).
class BASE_EXPORT ScopedAllowBaseSyncPrimitivesForTesting {
 public:
  ScopedAllowBaseSyncPrimitivesForTesting() EMPTY_BODY_IF_DCHECK_IS_OFF;
  ~ScopedAllowBaseSyncPrimitivesForTesting() EMPTY_BODY_IF_DCHECK_IS_OFF;

 private:
#if DCHECK_IS_ON()
  const bool was_disallowed_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowBaseSyncPrimitivesForTesting);
};

namespace internal {

// Asserts that waiting on a //base sync primitive is allowed in the current
// scope.
INLINE_IF_DCHECK_IS_OFF void AssertBaseSyncPrimitivesAllowed()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

// Resets all thread restrictions on the current thread.
INLINE_IF_DCHECK_IS_OFF void ResetThreadRestrictionsForTesting()
    EMPTY_BODY_IF_DCHECK_IS_OFF;

}  // namespace internal

class BASE_EXPORT ThreadRestrictions {
 public:
  // Constructing a ScopedAllowIO temporarily allows IO for the current
  // thread.  Doing this is almost certainly always incorrect.
  //
  // DEPRECATED. Use ScopedAllowBlocking(ForTesting).
  class BASE_EXPORT ScopedAllowIO {
   public:
    ScopedAllowIO() EMPTY_BODY_IF_DCHECK_IS_OFF;
    ~ScopedAllowIO() EMPTY_BODY_IF_DCHECK_IS_OFF;

   private:
#if DCHECK_IS_ON()
    const bool was_allowed_;
#endif

    DISALLOW_COPY_AND_ASSIGN(ScopedAllowIO);
  };

#if DCHECK_IS_ON()
  // Set whether the current thread to make IO calls.
  // Threads start out in the *allowed* state.
  // Returns the previous value.
  //
  // DEPRECATED. Use ScopedAllowBlocking(ForTesting) or ScopedDisallowBlocking.
  static bool SetIOAllowed(bool allowed);

  // Set whether the current thread can use singletons.  Returns the previous
  // value.
  static bool SetSingletonAllowed(bool allowed);

  // Check whether the current thread is allowed to use singletons (Singleton /
  // LazyInstance).  DCHECKs if not.
  static void AssertSingletonAllowed();

  // Disable waiting on the current thread. Threads start out in the *allowed*
  // state. Returns the previous value.
  //
  // DEPRECATED. Use DisallowBaseSyncPrimitives.
  static void DisallowWaiting();
#else
  // Inline the empty definitions of these functions so that they can be
  // compiled out.
  static bool SetIOAllowed(bool allowed) { return true; }
  static bool SetSingletonAllowed(bool allowed) { return true; }
  static void AssertSingletonAllowed() {}
  static void DisallowWaiting() {}
#endif

 private:
  // DO NOT ADD ANY OTHER FRIEND STATEMENTS, talk to jam or brettw first.
  // BEGIN ALLOWED USAGE.
  friend class android_webview::AwFormDatabaseService;
  friend class android_webview::CookieManager;
  friend class base::StackSamplingProfiler;
  friend class content::BrowserMainLoop;
  friend class content::BrowserShutdownProfileDumper;
  friend class content::BrowserSurfaceViewManager;
  friend class content::BrowserTestBase;
  friend class content::NestedMessagePumpAndroid;
  friend class content::ScopedAllowWaitForAndroidLayoutTests;
  friend class content::ScopedAllowWaitForDebugURL;
  friend class ::HistogramSynchronizer;
  friend class internal::TaskTracker;
  friend class cc::CompletionEvent;
  friend class cc::SingleThreadTaskGraphRunner;
  friend class content::CategorizedWorkerPool;
  friend class remoting::AutoThread;
  friend class ui::WindowResizeHelperMac;
  friend class MessagePumpDefault;
  friend class SimpleThread;
  friend class Thread;
  friend class ThreadTestHelper;
  friend class PlatformThread;
  friend class android::JavaHandlerThread;
  friend class mojo::SyncCallRestrictions;
  friend class mojo::edk::ScopedIPCSupport;
  friend class ui::CommandBufferClientImpl;
  friend class ui::CommandBufferLocal;
  friend class ui::GpuState;

  // END ALLOWED USAGE.
  // BEGIN USAGE THAT NEEDS TO BE FIXED.
  friend class ::chromeos::BlockingMethodCaller;  // http://crbug.com/125360
  friend class ::chromeos::system::StatisticsProviderImpl;  // http://crbug.com/125385
  friend class chrome_browser_net::Predictor;     // http://crbug.com/78451
  friend class
      content::BrowserGpuChannelHostFactory;      // http://crbug.com/125248
  friend class
      content::BrowserGpuMemoryBufferManager;     // http://crbug.com/420368
  friend class content::TextInputClientMac;       // http://crbug.com/121917
  friend class dbus::Bus;                         // http://crbug.com/125222
  friend class disk_cache::BackendImpl;           // http://crbug.com/74623
  friend class disk_cache::InFlightIO;            // http://crbug.com/74623
  friend class gpu::GpuChannelHost;               // http://crbug.com/125264
  friend class net::internal::AddressTrackerLinux;  // http://crbug.com/125097
  friend class net::NetworkChangeNotifierMac;     // http://crbug.com/125097
  friend class ::BrowserProcessImpl;              // http://crbug.com/125207
  friend class ::NativeBackendKWallet;            // http://crbug.com/125331
#if !defined(OFFICIAL_BUILD)
  friend class content::SoftwareOutputDeviceMus;  // Interim non-production code
#endif
  friend class views::ScreenMus;
  friend class viz::ServerGpuMemoryBufferManager;
// END USAGE THAT NEEDS TO BE FIXED.

#if DCHECK_IS_ON()
  // DEPRECATED. Use ScopedAllowBaseSyncPrimitives.
  static bool SetWaitAllowed(bool allowed);
#else
  static bool SetWaitAllowed(bool allowed) { return true; }
#endif

  // Constructing a ScopedAllowWait temporarily allows waiting on the current
  // thread.  Doing this is almost always incorrect, which is why we limit who
  // can use this through friend. If you find yourself needing to use this, find
  // another way. Talk to jam or brettw.
  //
  // DEPRECATED. Use ScopedAllowBaseSyncPrimitives.
  class BASE_EXPORT ScopedAllowWait {
   public:
    ScopedAllowWait() EMPTY_BODY_IF_DCHECK_IS_OFF;
    ~ScopedAllowWait() EMPTY_BODY_IF_DCHECK_IS_OFF;

   private:
#if DCHECK_IS_ON()
    const bool was_allowed_;
#endif

    DISALLOW_COPY_AND_ASSIGN(ScopedAllowWait);
  };

  DISALLOW_IMPLICIT_CONSTRUCTORS(ThreadRestrictions);
};

}  // namespace base

#endif  // BASE_THREADING_THREAD_RESTRICTIONS_H_
