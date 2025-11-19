// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPressure provides static APIs for handling memory pressure on
// platforms that have such signals, such as Android and ChromeOS.
// The app will try to discard buffers that aren't deemed essential (individual
// modules will implement their own policy).

#ifndef BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_
#define BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_

#include <memory>
#include <variant>

#include "base/base_export.h"
#include "base/location.h"
#include "base/memory/memory_pressure_level.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_checker.h"

namespace base {

class SingleThreadTaskRunner;

enum class MemoryPressureListenerTag {
  kTest = 0,
  kHangWatcher = 1,
  kMemBackend = 2,
  kLevelDb = 3,
  kSSLClientSessionCache = 4,
  kVulkanInProcessContextProvider = 5,
  kDemuxerManager = 6,
  kFrameEvictionManager = 7,
  kSlopBucket = 8,
  kDiscardableSharedMemoryManager = 9,
  kSharedStorageManager = 10,
  kStagingBufferPool = 11,
  kSharedDictionaryStorageOnDisk = 12,
  kHttpNetworkSession = 13,
  kBlobMemoryController = 14,
  kQuicSessionPool = 15,
  kImageDecodingStore = 16,
  kCompositorGpuThread = 17,
  kApplicationBreadcrumbsLogger = 18,
  kSkiaOutputSurfaceImpl = 19,
  kGpuImageDecodeCache = 20,
  kResourcePool = 21,
  kOnDeviceTailModelService = 22,
  kGpuChannelManager = 23,
  // Deprecated.
  // kSharedDictionaryManagerOnDisk = 24,
  kSharedDictionaryManager = 25,
  kHistoryBackend = 26,
  kMediaUrlIndex = 27,
  kBFCachePolicy = 28,
  kLayerTreeHostImpl = 29,
  kCacheStorageManager = 30,
  kPlayerCompositorDelegate = 31,
  kNetworkServiceClient = 32,
  kGpuChildThread = 33,
  kNavigationEntryScreenshotManager = 34,
  kGlicKeyedService = 35,
  kRenderThreadImpl = 36,
  kSpareRenderProcessHostManagerImpl = 37,
  kDOMStorageContextWrapper = 38,
  kGpuProcessHost = 39,
  kPrerenderHostRegistry = 40,
  kUrgentPageDiscardingPolicy = 41,
  kTabLoader = 42,
  kBackgroundTabLoadingPolicy = 43,
  kThumbnailCache = 44,
  kUserspaceSwapPolicy = 45,
  kWorkingSetTrimmerPolicyChromeOS = 46,
  kLruRendererCache = 47,
  kCastMemoryPressureControllerImpl = 48,
  kFontGlobalContext = 49,
  kClientDiscardableSharedMemoryManager = 50,
  kMemoryReclaimerPressureListener = 51,
  kSkiaGraphicsPressureListener = 52,
  kBlinkIsolatesPressureListener = 53,
  kUniqueFontSelector = 54,
  kParkableStringManager = 55,
  kPlainTextPainter = 56,
  kMemoryCache = 57,
  kResource = 58,
  kResourceFetcher = 59,
};

// To start listening, derive from MemoryPressureListener, and use
// MemoryPressureListenerRegistration to register your class with the global
// registry. To stop listening, simply delete the registration object, which
// will ensure that `OnMemoryPressure()` will no longer be invoked. The
// implementation guarantees that the notification will always be received on
// the thread that created the listener.
//
// If the registration can't be done on the main thread of the process, then
// AsyncMemoryPressureListenerRegistration must be used, and notifications will
// be asynchronous as well.
//
// Please see notes in MemoryPressureLevel enum below: some levels are
// absolutely critical, and if not enough memory is returned to the system,
// it'll potentially kill the app, and then later the app will have to be
// cold-started.
//
// Example usage:
//
// class ExampleMemoryPressureListener : public MemoryPressureListener {
//  public:
//   ExampleMemoryPressureListener()
//       : memory_pressure_listener_registration_(tag, this) {}
//   ~ExampleMemoryPressureListener() override;
//
//   // MemoryPressureListener:
//   void OnMemoryPressure(MemoryPressureLevel level) override {
//     // Do something with `level`.
//   }
//
//  private:
//   MemoryPressureListenerRegistration memory_pressure_listener_registration_;
// };

class BASE_EXPORT MemoryPressureListener : public CheckedObserver {
 public:
  // Intended for use by the platform specific implementation.
  // Note: This simply forwards the call to MemoryPressureListenerRegistry to
  // avoid the need to refactor the whole codebase.
  static void NotifyMemoryPressure(MemoryPressureLevel memory_pressure_level);

  // These methods should not be used anywhere else but in memory measurement
  // code, where they are intended to maintain stable conditions across
  // measurements.
  // Note: This simply forwards the call to MemoryPressureListenerRegistry to
  // avoid the need to refactor the whole codebase.
  static bool AreNotificationsSuppressed();
  static void SetNotificationsSuppressed(bool suppressed);
  static void SimulatePressureNotification(
      MemoryPressureLevel memory_pressure_level);
  // Invokes `SimulatePressureNotification` asynchronously on the main thread,
  // ensuring that any pending registration tasks have completed by the time it
  // runs.
  static void SimulatePressureNotificationAsync(
      MemoryPressureLevel memory_pressure_level);

  virtual void OnMemoryPressure(MemoryPressureLevel memory_pressure_level) = 0;
};

// Used for listeners that live on the main thread and must be called
// synchronously. Prefer using MemoryPressureListenerRegistration as this will
// eventually be removed.
class BASE_EXPORT SyncMemoryPressureListenerRegistration {
 public:
  SyncMemoryPressureListenerRegistration(
      MemoryPressureListenerTag,
      MemoryPressureListener* memory_pressure_listener);

  SyncMemoryPressureListenerRegistration(
      const SyncMemoryPressureListenerRegistration&) = delete;
  SyncMemoryPressureListenerRegistration& operator=(
      const SyncMemoryPressureListenerRegistration&) = delete;

  ~SyncMemoryPressureListenerRegistration();

  void Notify(MemoryPressureLevel memory_pressure_level);

  MemoryPressureListenerTag tag() { return tag_; }

 private:
  MemoryPressureListenerTag tag_;

  raw_ptr<MemoryPressureListener> memory_pressure_listener_
      GUARDED_BY_CONTEXT(thread_checker_);

  THREAD_CHECKER(thread_checker_);
};

// Used for listeners that can exists on sequences other than the main thread
// and don't need to be called synchronously.
class BASE_EXPORT AsyncMemoryPressureListenerRegistration {
 public:
  AsyncMemoryPressureListenerRegistration(
      const base::Location& creation_location,
      MemoryPressureListenerTag tag,
      MemoryPressureListener* memory_pressure_listener);

  AsyncMemoryPressureListenerRegistration(
      const AsyncMemoryPressureListenerRegistration&) = delete;
  AsyncMemoryPressureListenerRegistration& operator=(
      const AsyncMemoryPressureListenerRegistration&) = delete;

  ~AsyncMemoryPressureListenerRegistration();

 private:
  class MainThread;

  void Notify(MemoryPressureLevel memory_pressure_level);

  raw_ptr<MemoryPressureListener> memory_pressure_listener_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Handle to the main thread's task runner. This is cached because it might no
  // longer be registered at the time this instance is destroyed.
  scoped_refptr<SingleThreadTaskRunner> main_thread_task_runner_;

  // Parts of this class that lives on the main thread.
  std::unique_ptr<MainThread> main_thread_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const base::Location creation_location_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  WeakPtrFactory<AsyncMemoryPressureListenerRegistration> weak_ptr_factory_{
      this};
};

// Used for listeners that live on the main thread. Can be call synchronously or
// asynchronously.
// Note: In the future, this will be always called synchronously.
class BASE_EXPORT MemoryPressureListenerRegistration {
 public:
  MemoryPressureListenerRegistration(
      const Location& creation_location,
      MemoryPressureListenerTag tag,
      MemoryPressureListener* memory_pressure_listener);

  MemoryPressureListenerRegistration(
      const MemoryPressureListenerRegistration&) = delete;
  MemoryPressureListenerRegistration& operator=(
      const MemoryPressureListenerRegistration&) = delete;

  ~MemoryPressureListenerRegistration();

 private:
  std::variant<SyncMemoryPressureListenerRegistration,
               AsyncMemoryPressureListenerRegistration>
      listener_;
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_LISTENER_H_
