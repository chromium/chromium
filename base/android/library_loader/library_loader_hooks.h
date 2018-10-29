// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LIBRARY_LOADER_LIBRARY_LOADER_HOOKS_H_
#define BASE_ANDROID_LIBRARY_LOADER_LIBRARY_LOADER_HOOKS_H_

#include <jni.h>

#include "base/base_export.h"
#include "base/callback.h"

namespace base {
namespace android {

// The process the shared library is loaded in.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base.library_loader
enum LibraryProcessType {
  // The LibraryLoad has not been initialized.
  PROCESS_UNINITIALIZED = 0,
  // Shared library is running in browser process.
  PROCESS_BROWSER = 1,
  // Shared library is running in child process.
  PROCESS_CHILD = 2,
  // Shared library is running in the app that uses webview.
  PROCESS_WEBVIEW = 3,
  // Shared library is running in child process as part of webview.
  PROCESS_WEBVIEW_CHILD = 4,
};

typedef bool NativeInitializationHook(LibraryProcessType library_process_type);

BASE_EXPORT void SetNativeInitializationHook(
    NativeInitializationHook native_initialization_hook);

// Record any pending renderer histogram value as histograms.  Pending values
// are set by RegisterChromiumAndroidLinkerRendererHistogram and
// RegisterLibraryPreloaderRendererHistogram.
BASE_EXPORT void RecordLibraryLoaderRendererHistograms();

// Typedef for hook function to be called (indirectly from Java) once the
// libraries are loaded. The hook function should register the JNI bindings
// required to start the application. It should return true for success and
// false for failure.
// Note: this can't use base::Callback because there is no way of initializing
// the default callback without using static objects, which we forbid.
typedef bool LibraryLoadedHook(JNIEnv* env,
                               jclass clazz,
                               LibraryProcessType library_process_type);

// Set the hook function to be called (from Java) once the libraries are loaded.
// SetLibraryLoadedHook may only be called from JNI_OnLoad. The hook function
// should register the JNI bindings required to start the application.

BASE_EXPORT void SetLibraryLoadedHook(LibraryLoadedHook* func);

// Pass the version name to the loader. This used to check that the library
// version matches the version expected by Java before completing JNI
// registration.
// Note: argument must remain valid at least until library loading is complete.
BASE_EXPORT void SetVersionNumber(const char* version_number);

// Call on exit to delete the AtExitManager which OnLibraryLoadedOnUIThread
// created.
BASE_EXPORT void LibraryLoaderExitHook();

// Initialize AtExitManager, this must be done at the begining of loading
// shared library.
void InitAtExitManager();

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_LIBRARY_LOADER_LIBRARY_LOADER_HOOKS_H_
