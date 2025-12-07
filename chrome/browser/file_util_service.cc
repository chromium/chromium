// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_util_service.h"

#include "chrome/grit/generated_resources.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/service_process_host.h"

// Class allows us to friend the passkeys we need to launch the service.
class FileUtilServiceLauncher {
 public:
  static mojo::PendingRemote<chrome::mojom::FileUtilService>
  LaunchFileUtilService() {
    mojo::PendingRemote<chrome::mojom::FileUtilService> remote;
    content::ServiceProcessHost::Launch<chrome::mojom::FileUtilService>(
        remote.InitWithNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName(IDS_UTILITY_PROCESS_FILE_UTILITY_NAME)
            .Pass());
    return remote;
  }
};

mojo::PendingRemote<chrome::mojom::FileUtilService> LaunchFileUtilService() {
  return FileUtilServiceLauncher::LaunchFileUtilService();
}
