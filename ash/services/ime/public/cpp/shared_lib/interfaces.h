// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_IME_PUBLIC_CPP_SHARED_LIB_INTERFACES_H_
#define ASH_SERVICES_IME_PUBLIC_CPP_SHARED_LIB_INTERFACES_H_

#include <stddef.h>
#include <stdint.h>

// Introduce interfaces between the IME Mojo service and its IME shared library
// which is loaded dynamically during runtime.
//
// Prerequisite: Compilers of both IME service and IME shared library should
// follow the same or similar ABIs, so that their generated code should be
// interoperable.
//
// Please note that the IME shared library is not likely compiled with the same
// compiler as the IME Mojo service. IME shared library's creater should be
// responsible for compiling it in a compiler with the same or similar ABIs.
//
// For polymorphism, the interfaces should be abstract base classes with pure
// virtual methods. An interface will be implemented in one end and called on
// the other end.
//
//                          +-----------+
//                    .---> | Interface | <---.
//         depends on |     +-----------+     | depends on
//                    |                       |
//           +----------------+         +-------------+
//           | Shared Library |         | IME Service |
//           +----------------+         +-------------+
//
// For best practices on creating/modifying these interfaces.
//
// 1, Stick with "C" style arguments and return types over these interfaces,
// because the "C" ABIs are much more stable. E.g. std structures over these
// interfaces is not likely layout-compatible between compilers.
//
// 2, Every interface here should only contain pure virtual methods, except
// its destructor.
//
// 3, Protect destructors of these interfaces to discourage clients from
// deleting interface. Providing a Destroy function if needed.
//
// 4, Always add new methods at the end of an interface, and do not add new
// virtual overloaded functions (methods with the same name).
//
// 5, Document the ownership of these interfaces' parameters to avoid memory
// leaks or overflows.
//
// IME service and its IME shared library need to be recompiled when these
// interfaces change. Keep these interfaces as stable as possible, so making
// changes to the implementation of an interface will not need to recompile the
// caller end.
//
// And it's important to keep any unnecessary information out of this header.

// Forward declare MojoSystemThunks to keep this file free of direct
// dependencies on Chromium, making it easier to use this file outside of the
// Chromium repo. When using this file, consumers should also #include their own
// copy of the MojoSystemThunks struct definition.
struct MojoSystemThunks;

namespace chromeos {
namespace ime {

// Callback upon async completion of DownloadToFile(), passing the originally
// issued |request_id| (as returned by DownloadToFile()) and an |error_code| (as
// defined at
// https://cs.chromium.org/chromium/src/net/base/net_error_list.h?rcl=f9c935b73381772d508eebba1e216c437139d475).
typedef void (*ImeCrosDownloadCallback)(int request_id, int status_code);

// A simple downloading callback.
typedef void (*SimpleDownloadCallback)(int status_code, const char* file_path);

// A simple downloading callback with the downloading URL as return.
typedef void (*SimpleDownloadCallbackV2)(int status_code,
                                         const char* url,
                                         const char* file_path);

// A function pointer of a sequenced task.
typedef void (*ImeSequencedTask)(int task_id);

// A logger function pointer from chrome.
typedef void (*ChromeLoggerFunc)(int severity, const char* message);

// Based on RequestPriority defined at
// https://cs.chromium.org/chromium/src/net/base/request_priority.h?rcl=f9c935b73381772d508eebba1e216c437139d475
enum DownloadPriority {
  THROTTLED = 0,
  MINIMUM_PRIORITY = THROTTLED,
  IDLE = 1,
  LOWEST = 2,
  DEFAULT_PRIORITY = LOWEST,
  LOW = 3,
  MEDIUM = 4,
  HIGHEST = 5,
  MAXIMUM_PRIORITY = HIGHEST,
};

// Extendable extra options for a download.
struct DownloadOptions {
  // Duration (in milliseconds) to wait before giving up on the download and
  // considering it an error. Negative value means it can take indefinitely.
  long timeout_ms;

  // Priority level for the download.
  DownloadPriority priority;

  // Max number of times to retry a download (exclusive of the initial attempt).
  unsigned int max_retries;

  // Always add more stuff at the end only. Just like protobuf, refrain from
  // deleting or re-ordering for maximal API stability and backward
  // compatibility. Simply mark fields as "deprecated" if need be.
};

// Provides CrOS network download service to the shared library.
class ImeCrosDownloader {
 protected:
  virtual ~ImeCrosDownloader() = default;

 public:
  // Download data from the given |url| and store into a file located at the
  // given |file_path|, using the specified download |options|. The method
  // returns a |request_id| (unique among those issued by the same Downloader),
  // while actual download operation takes place asynchronously. Upon async
  // completion of the download (either success or failure), the given
  // |callback| function will be invoked, passing a matching |request_id| and
  // the status via an |error_code|. All arguments are const and completely
  // owned by the caller at all times; they should remain alive till the sync
  // return of this method.
  virtual int DownloadToFile(const char* url,
                             const DownloadOptions& options,
                             const char* file_path,
                             ImeCrosDownloadCallback callback) = 0;

  // Cancel the download whose |request_id| is given (|request_id| is issued
  // in the return value of each DownloadToFile() call). The callback of a
  // cancelled download will never be invoked. If the |request_id| is invalid
  // or belongs to an already completed download (either success or failure),
  // this method will just no-op.
  virtual void Cancel(int request_id) = 0;
};

// This defines the `ImeCrosPlatform` interface, which is used throughout the
// shared library to manage platform-specific data/operations.
//
// This class should be provided by the IME service before creating an
// `ImeEngineMainEntry` and be always owned by the IME service.
class ImeCrosPlatform {
 protected:
  virtual ~ImeCrosPlatform() = default;

 public:
  // The three methods below are Getters of the local data directories on the
  // platform. It's possible for the IME service to be running in a mode where
  // some local directories are unavailable, in which case these directories
  // will be empty.
  //
  // The returned pointer must remain valid until the `Platform` is destroyed.

  // Get the local IME bundle directory, which is read-only.
  virtual const char* GetImeBundleDir() = 0;

  // Get the IME global directory, which is accessible to all users.
  virtual const char* GetImeGlobalDir() = 0;

  // Get the local IME directory in home directory of the active user, which
  // is only accessible to the user itself.
  virtual const char* GetImeUserHomeDir() = 0;

  // Get the Downloader that provides CrOS network download service. Ownership
  // of the returned Downloader instance is never transferred, i.e. it remains
  // owned by the IME service / Platform at all times.
  virtual ImeCrosDownloader* GetDownloader() = 0;

  // A shortcut for starting a downloading by the network |SimpleURLLoader|.
  // Each SimpleDownloadToFile can only be used for a single request.
  // Make a call after the previous task completes or cancels.
  virtual int SimpleDownloadToFile(const char* url,
                                   const char* file_path,
                                   SimpleDownloadCallback callback) = 0;

  // This is used for decoder to run some Mojo-specific operation which is
  // required to run in the thread creating its remote.
  virtual void RunInMainSequence(ImeSequencedTask task, int task_id) = 0;

  // Returns whether a Chrome OS experimental feature is enabled or not.
  virtual bool IsFeatureEnabled(const char* feature_name) = 0;

  // Version 2 for |SimpleDownloadToFile|. Downloading URL is added into its
  // callback.
  virtual int SimpleDownloadToFileV2(const char* url,
                                     const char* file_path,
                                     SimpleDownloadCallbackV2 callback) = 0;

  // Returns a pointer to the Mojo system thunks.
  // The shared library can use this pointer for its own Mojo environment in
  // order to communicate directly with the browser process.
  // MojoSystemThunks has a stable ABI, hence it is safe to use it from the
  // shared library
  virtual const MojoSystemThunks* GetMojoSystemThunks() = 0;

  // TODO(https://crbug.com/837156): Provide Logger for main entry.
};

// The wrapper of Mojo InterfacePtr on an IME client.
//
// This is used to send messages to connected IME client from an IME engine.
// IME service will create then pass it to the engine.
class ImeClientDelegate {
 protected:
  virtual ~ImeClientDelegate() = default;

 public:
  // Returns the c_str() of the internal IME specification of ImeClientDelegate.
  // The IME specification will be invalidated by its `Destroy` method.
  virtual const char* ImeSpec() = 0;

  // Process response data from the engine instance in its connected IME client.
  // The data will be invalidated by the engine soon after this call.
  virtual void Process(const uint8_t* data, size_t size) = 0;

  // Destroy the `ImeClientDelegate` instance, which is called in the shared
  // library when the bound engine is destroyed.
  virtual void Destroy() = 0;
};

// For use when bridging logs logged in IME shared library to Chrome logging.
typedef void (*ImeEngineLoggerSetterFn)(ChromeLoggerFunc);

// Functions blow are exported by the IME decoder shared library that we expose
// through a loader.

// Initialize the IME decoder.
//
// Any user of IME decoder must make a call on this function before any others.
//
// The provided `ImeCrosPlatform` must remain valid during the whole life of
// shared libraray
typedef void (*ImeDecoderInitOnceFn)(ImeCrosPlatform*);

// Returns whether a specific IME is supported by this IME shared library.
// The argument is the specfiation name of an IME, and the caller should
// explicitly know the IME engine's naming rules.
typedef bool (*ImeDecoderSupportsFn)(const char*);

// Activate an IME instance in the shared library with an IME specfiation name
// and a bound `ImeClientDelegate` which is a channel from the IME instance to
// its client.
//
// The ownership of `ImeClientDelegate` will be passed to the IME instance.
typedef bool (*ImeDecoderActivateImeFn)(const char*, ImeClientDelegate*);

// Process IME events by the activated IME instance.
// The data passed in  should be invalidated by the IME instance soon after it's
// consumed.
typedef void (*ImeDecoderProcessFn)(const uint8_t*, size_t);

// Release resources used by the IME decoder.
typedef void (*ImeDecoderCloseFn)();

// Set up a direct connection with the shared library via Mojo.
typedef bool (*ConnectToInputMethodFn)(const char*,
                                       uint32_t,
                                       uint32_t,
                                       uint32_t);

// Whether there's a direct connection with Mojo.
typedef bool (*IsInputMethodConnectedFn)();

}  // namespace ime
}  // namespace chromeos

#endif  // ASH_SERVICES_IME_PUBLIC_CPP_SHARED_LIB_INTERFACES_H_
