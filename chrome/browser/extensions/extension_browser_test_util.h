// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"

namespace extensions::browser_test_util {

// Different types of extension's lazy background contexts used in some tests.
enum class ContextType {
  // TODO(crbug.com/40785880): Get rid of this value when we can use
  // std::optional in the LoadOptions struct.
  // No specific context type.
  kNone,
  // A non-persistent background page/JS based extension.
  kEventPage,
  // A Service Worker based extension.
  kServiceWorker,
  // A Service Worker based extension that uses MV2.
  kServiceWorkerMV2,
  // An extension with a persistent background page.
  kPersistentBackground,
  // Use the value from the manifest. This is used when the test
  // has been parameterized but the particular extension should
  // be loaded without using the parameterized type. Typically,
  // this is used when a test loads another extension that is
  // not parameterized.
  kFromManifest,
};

struct LoadOptions {
  // Allows the extension to run in incognito mode.
  bool allow_in_incognito = false;

  // Allows file access for the extension.
  bool allow_file_access = false;

  // Doesn't fail when the loaded manifest has warnings (should only be used
  // when testing deprecated features).
  bool ignore_manifest_warnings = false;

  // Waits for extension renderers to fully load.
  bool wait_for_renderers = true;

  // An optional install param.
  const char* install_param = nullptr;

  // If this is a Service Worker-based extension, wait for the
  // Service Worker's registration to be stored before returning.
  bool wait_for_registration_stored = false;

  // Loads the extension with location COMPONENT.
  bool load_as_component = false;

  // Changes the "manifest_version" manifest key to 3. Note as of now, this
  // doesn't make any other changes to convert the extension to MV3 other than
  // changing the integer value in the manifest.
  bool load_as_manifest_version_3 = false;

  // Used to force loading the extension with a particular background type.
  // Currently this only support loading an extension as using a service
  // worker.
  ContextType context_type = ContextType::kNone;
};

bool IsServiceWorkerContext(ContextType context_type);

// Modifies extension at `input_path` as dictated by `options`. On success,
// returns true and populates `out_path`. On failure, false is returned.
// `test_pre_count': number of "PRE_" prefixes in the test name
// `temp_dir_path`: temporary directory for extension files.
bool ModifyExtensionIfNeeded(const LoadOptions& options,
                             ContextType context_type,
                             size_t test_pre_count,
                             const base::FilePath& temp_dir_path,
                             const base::FilePath& input_path,
                             base::FilePath* out_path);

}  // namespace extensions::browser_test_util

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_TEST_UTIL_H_
