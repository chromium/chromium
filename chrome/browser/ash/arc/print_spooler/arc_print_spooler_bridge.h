// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_BRIDGE_H_

#include "ash/components/arc/mojom/print_spooler.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class handles print related IPC from the ARC container and allows print
// jobs to be displayed and managed in Chrome print preview instead of the
// Android print UI.
class ArcPrintSpoolerBridge : public KeyedService,
                              public mojom::PrintSpoolerHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcPrintSpoolerBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcPrintSpoolerBridge(content::BrowserContext* context,
                        ArcBridgeService* bridge_service);

  ArcPrintSpoolerBridge(const ArcPrintSpoolerBridge&) = delete;
  ArcPrintSpoolerBridge& operator=(const ArcPrintSpoolerBridge&) = delete;

  ~ArcPrintSpoolerBridge() override;

  // mojom::PrintSpoolerHost:
  void StartPrintInCustomTab(
      mojo::ScopedHandle scoped_handle,
      int32_t task_id,
      mojo::PendingRemote<mojom::PrintSessionInstance> instance,
      StartPrintInCustomTabCallback callback) override;

  void OnPrintDocumentSaved(
      int32_t task_id,
      mojo::PendingRemote<mojom::PrintSessionInstance> instance,
      StartPrintInCustomTabCallback callback,
      base::FilePath file_path);

  static void EnsureFactoryBuilt();

 private:
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ArcPrintSpoolerBridge> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_PRINT_SPOOLER_ARC_PRINT_SPOOLER_BRIDGE_H_
