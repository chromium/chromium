// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net_benchmarking.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net_benchmarking.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

using content::BrowserThread;

namespace {

network::mojom::NetworkContext* GetNetworkContext(int render_process_id) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!render_process_host)
    return nullptr;
  return render_process_host->GetStoragePartition()->GetNetworkContext();
}

}  // namespace

NetBenchmarking::NetBenchmarking(
    base::WeakPtr<predictors::LoadingPredictor> loading_predictor,
    int render_process_id)
    : loading_predictor_(loading_predictor),
      render_process_id_(render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

NetBenchmarking::~NetBenchmarking() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

// static
void NetBenchmarking::Create(
    base::WeakPtr<predictors::LoadingPredictor> loading_predictor,
    int render_process_id,
    mojo::PendingReceiver<chrome::mojom::NetBenchmarking> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NetBenchmarking>(std::move(loading_predictor),
                                        render_process_id),
      std::move(receiver));
}

// static
bool NetBenchmarking::CheckBenchmarkingEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kEnableNetBenchmarking);
}

void NetBenchmarking::ClearCache(ClearCacheCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* network_context = GetNetworkContext(render_process_id_);
  CHECK(network_context);
  network_context->ClearHttpCache(base::Time(), base::Time(), nullptr,
                                  std::move(callback));
}

void NetBenchmarking::ClearHostResolverCache(
    ClearHostResolverCacheCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* network_context = GetNetworkContext(render_process_id_);
  CHECK(network_context);
  network_context->ClearHostCache(nullptr, std::move(callback));
}

void NetBenchmarking::CloseCurrentConnections(
    CloseCurrentConnectionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* network_context = GetNetworkContext(render_process_id_);
  CHECK(network_context);
  network_context->CloseAllConnections(std::move(callback));
}

void NetBenchmarking::ClearPredictorCache(
    ClearPredictorCacheCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (loading_predictor_)
    loading_predictor_->resource_prefetch_predictor()->DeleteAllUrls();
  std::move(callback).Run();
}
