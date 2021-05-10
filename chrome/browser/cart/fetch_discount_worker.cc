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
const int64_t kDelayFetchMs = 1800000;
const int kImmediateFetchMs = 0;

}  // namespace

CartLoader::CartLoader(Profile* profile)
    : cart_service_(CartServiceFactory::GetForProfile(profile)) {}

CartLoader::~CartLoader() = default;

void CartLoader::LoadAllCarts(CartDB::LoadCallback callback) {
  cart_service_->LoadAllActiveCarts(std::move(callback));
}

CartDiscountUpdater::CartDiscountUpdater(Profile* profile)
    : cart_service_(CartServiceFactory::GetForProfile(profile)) {}

CartDiscountUpdater::~CartDiscountUpdater() = default;

void CartDiscountUpdater::update() {}

CartLoaderAndUpdaterFactory::CartLoaderAndUpdaterFactory(Profile* profile)
    : profile_(profile) {}

CartLoaderAndUpdaterFactory::~CartLoaderAndUpdaterFactory() = default;

std::unique_ptr<CartLoader> CartLoaderAndUpdaterFactory::createCartLoader() {
  return std::make_unique<CartLoader>(profile_);
}

std::unique_ptr<CartDiscountUpdater>
CartLoaderAndUpdaterFactory::createCartDiscountUpdater() {
  return std::make_unique<CartDiscountUpdater>(profile_);
}

FetchDiscountWorker::FetchDiscountWorker(
    scoped_refptr<network::SharedURLLoaderFactory>
        browserProcessURLLoaderFactory,
    std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory,
    std::unique_ptr<CartLoaderAndUpdaterFactory>
        cart_loader_and_updater_factory)
    : browserProcessURLLoaderFactory_(browserProcessURLLoaderFactory),
      fetcher_factory_(std::move(fetcher_factory)),
      cart_loader_and_updater_factory_(
          std::move(cart_loader_and_updater_factory)) {
  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});
}

FetchDiscountWorker::~FetchDiscountWorker() = default;

void FetchDiscountWorker::Start(base::TimeDelta delay) {
  // Post a delay task to avoid an infinite loop for creating the CartService.
  // Since CartLoader and CartDiscountUpdater both depend on CartService.
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FetchDiscountWorker::PrepareToFetch,
                         weak_ptr_factory_.GetWeakPtr(), kImmediateFetchMs),
          delay);
}

void FetchDiscountWorker::PrepareToFetch(unsigned int delay_fetch_ms) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Load all active carts.
  auto cart_loaded_callback =
      base::BindOnce(&FetchDiscountWorker::ReadyToFetch,
                     weak_ptr_factory_.GetWeakPtr(), delay_fetch_ms);
  auto loader = cart_loader_and_updater_factory_->createCartLoader();
  loader->LoadAllCarts(std::move(cart_loaded_callback));
}

void FetchDiscountWorker::ReadyToFetch(
    int delay_fetch_ms,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto pending_factory = browserProcessURLLoaderFactory_->Clone();
  auto fetcher = fetcher_factory_->createFetcher();
  auto done_fetching_callback =
      base::BindOnce(&FetchDiscountWorker::AfterDiscountFetched,
                     weak_ptr_factory_.GetWeakPtr());

  backend_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FetchInBackground, std::move(pending_factory),
                     std::move(fetcher), std::move(done_fetching_callback)),
      base::TimeDelta::FromMilliseconds(delay_fetch_ms));
}

void FetchDiscountWorker::FetchInBackground(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
    std::unique_ptr<CartDiscountFetcher> fetcher,
    AfterFetchingCallback after_fetching_callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto done_fetching_callback = base::BindOnce(
      &DoneFetchingInBackground, std::move(after_fetching_callback));
  fetcher->Fetch(std::move(pending_factory), std::move(done_fetching_callback));
}

// TODO(meiliang): Follow up to use BindPostTask.
void FetchDiscountWorker::DoneFetchingInBackground(
    AfterFetchingCallback after_fetching_callback,
    CartDiscountFetcher::CartDiscountMap discounts) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     [](AfterFetchingCallback callback,
                        CartDiscountFetcher::CartDiscountMap map) {
                       std::move(callback).Run(std::move(map));
                     },
                     std::move(after_fetching_callback), std::move(discounts)));
}

void FetchDiscountWorker::AfterDiscountFetched(
    CartDiscountFetcher::CartDiscountMap discounts) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto update_discount_callback =
      base::BindOnce(&FetchDiscountWorker::OnUpdatingDiscounts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(discounts));
  auto loader = cart_loader_and_updater_factory_->createCartLoader();
  loader->LoadAllCarts(std::move(update_discount_callback));
}

void FetchDiscountWorker::OnUpdatingDiscounts(
    CartDiscountFetcher::CartDiscountMap discounts,
    bool success,
    std::vector<CartDB::KeyAndValue> proto_pairs) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // TODO(meiliang): Iterate over |proto_pairs| and update it based on
  // |discounts|.

  // Continue to work
  PrepareToFetch(kDelayFetchMs);
}
