// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/print_spooler/arc_print_spooler_bridge.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/print_spooler/arc_print_spooler_util.h"
#include "chrome/browser/ash/arc/print_spooler/print_session_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace arc {
namespace {

// Singleton factory for ArcPrintSpoolerBridge.
class ArcPrintSpoolerBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcPrintSpoolerBridge, ArcPrintSpoolerBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcPrintSpoolerBridgeFactory";

  static ArcPrintSpoolerBridgeFactory* GetInstance() {
    return base::Singleton<ArcPrintSpoolerBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcPrintSpoolerBridgeFactory>;
  ArcPrintSpoolerBridgeFactory() = default;
  ~ArcPrintSpoolerBridgeFactory() override = default;
};

}  // namespace

// static
ArcPrintSpoolerBridge* ArcPrintSpoolerBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcPrintSpoolerBridgeFactory::GetForBrowserContext(context);
}

ArcPrintSpoolerBridge::ArcPrintSpoolerBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      profile_(Profile::FromBrowserContext(context)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->print_spooler()->SetHost(this);
}

ArcPrintSpoolerBridge::~ArcPrintSpoolerBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->print_spooler()->SetHost(nullptr);
}

void ArcPrintSpoolerBridge::StartPrintInCustomTabDeprecated(
    mojo::ScopedHandle scoped_handle,
    int32_t task_id,
    int32_t surface_id,
    int32_t top_margin,
    mojo::PendingRemote<mojom::PrintSessionInstance> instance,
    StartPrintInCustomTabCallback callback) {
  StartPrintInCustomTab(std::move(scoped_handle), task_id, std::move(instance),
                        std::move(callback));
}

void ArcPrintSpoolerBridge::StartPrintInCustomTab(
    mojo::ScopedHandle scoped_handle,
    int32_t task_id,
    mojo::PendingRemote<mojom::PrintSessionInstance> instance,
    StartPrintInCustomTabCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SavePrintDocument, std::move(scoped_handle)),
      base::BindOnce(&ArcPrintSpoolerBridge::OnPrintDocumentSaved,
                     weak_ptr_factory_.GetWeakPtr(), task_id,
                     std::move(instance), std::move(callback)));
}

void ArcPrintSpoolerBridge::OnPrintDocumentSaved(
    int32_t task_id,
    mojo::PendingRemote<mojom::PrintSessionInstance> instance,
    StartPrintInCustomTabCallback callback,
    base::FilePath file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (file_path.empty()) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  GURL url = net::FilePathToFileURL(base::MakeAbsoluteFilePath(file_path));

  aura::Window* arc_window = GetArcWindow(task_id);
  if (!arc_window) {
    LOG(ERROR) << "No ARC window with the specified task ID " << task_id;
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  auto web_contents = CreateArcCustomTabWebContents(profile_, url);
  std::move(callback).Run(PrintSessionImpl::Create(
      std::move(web_contents), arc_window, std::move(instance)));
}

}  // namespace arc
