// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_UTIL_SERVICE_H_
#define CHROME_BROWSER_FILE_UTIL_SERVICE_H_

#include "chrome/services/file_util/public/mojom/file_util_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

// Launches a new instance of the FileUtil service in an isolated, sandboxed
// process and returns a remote interface to control the service. The lifetime
// of the process is tied to that of the remote. May be called from any thread.
mojo::PendingRemote<chrome::mojom::FileUtilService> LaunchFileUtilService();

#endif  // CHROME_BROWSER_FILE_UTIL_SERVICE_H_
