// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_PROCESS_TYPE_H_
#define BASE_PROFILER_PROCESS_TYPE_H_

namespace base {

// TODO(crbug.com/354124876): Revisit whether these enums needs to live in
// //base, once the core logic has been moved.

// The type of process which is profiled.
enum class ProfilerProcessType {
  kUnknown,
  kBrowser,
  kRenderer,
  kGpu,
  kUtility,
  kZygote,
  kSandboxHelper,
  kPpapiPlugin,
  kNetworkService,

  kMax = kNetworkService,
};

// The type of thread which is profiled.
enum class ProfilerThreadType {
  kUnknown,

  // Each process has a 'main thread'. In the Browser process, the 'main
  // thread' is also often called the 'UI thread'.
  kMain,
  kIo,

  // Compositor thread (can be in both renderer and gpu processes).
  kCompositor,

  // Service worker thread.
  kServiceWorker,

  kMax = kServiceWorker,
};

}  // namespace base

#endif  // BASE_PROFILER_PROCESS_TYPE_H_
