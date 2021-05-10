// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/fetch_discount_worker.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {
// TODO(meiliang): Make it configurable via finch parameter.
// 30 minutes.
const int64_t DELAY_WORK_MS = 1800000;
}  // namespace

FetchDiscountWorker::FetchDiscountWorker(
    scoped_refptr<network::SharedURLLoaderFactory>
        browserProcessURLLoaderFactory,
    std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory)
    : browserProcessURLLoaderFactory_(browserProcessURLLoaderFactory),
      fetcher_factory_(std::move(fetcher_factory)) {
  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});
}

FetchDiscountWorker::~FetchDiscountWorker() = default;

void FetchDiscountWorker::Start() {
  PostDiscountFetchTask(0);
}

void FetchDiscountWorker::PostDiscountFetchTask(int delay_work_ms) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto pending_factory = browserProcessURLLoaderFactory_->Clone();

  auto continue_to_work_callback =
      base::BindOnce(&FetchDiscountWorker::PostDiscountFetchTask,
                     weak_ptr_factory_.GetWeakPtr(), DELAY_WORK_MS);

  std::unique_ptr<CartDiscountFetcher> fetcher =
      fetcher_factory_->createFetcher();

  backend_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DoWorkInBackground, std::move(pending_factory),
                     std::move(fetcher), std::move(continue_to_work_callback)),
      base::TimeDelta::FromMilliseconds(delay_work_ms));
}

void FetchDiscountWorker::DoWorkInBackground(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    std::unique_ptr<CartDiscountFetcher> fetcher,
    ContinueToWorkCallback continue_to_work_callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto on_work_finished_callback =
      base::BindOnce(&OnWorkFinished, std::move(continue_to_work_callback));

  // TODO(meiliang): load carts and call fetcher to get discounts for carts
  // CartDiscountFetcher fetcher;
  fetcher->Fetch(std::move(pending_factory),
                 std::move(on_work_finished_callback));
}

void FetchDiscountWorker::OnWorkFinished(
    ContinueToWorkCallback continue_to_work_callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // TODO(meiliang): Load and update carts with discount.

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(continue_to_work_callback));
}
