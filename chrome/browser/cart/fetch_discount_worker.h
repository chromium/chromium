// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_
#define CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/cart/cart_discount_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
class PendingSharedURLLoaderFactory;
}  // namespace network

// This is used to fetch discounts for active Carts in cart_db. It starts
// to work after calling #Start and continue to work util Chrome is finished.
// The flow looks as follow:
//   Start()
//     |
//     V
//   PostDiscountFetchTask() <----
//     |                         |
//     V                         |
//   DoWorkInBackground()        |
//     |                         |
//     V                         |
//   OnWorkFinished()  -----------
//
// TODO(meiliang): Add an API to allow ending the work earlier. e.g. when user
// has hidden the cart module.
class FetchDiscountWorker {
 public:
  FetchDiscountWorker(
      scoped_refptr<network::SharedURLLoaderFactory>
          browserProcessURLLoaderFactory,
      std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory);
  ~FetchDiscountWorker();
  // Starts the worker to work.
  void Start();

 private:
  using ContinueToWorkCallback = base::OnceCallback<void()>;

  scoped_refptr<network::SharedURLLoaderFactory>
      browserProcessURLLoaderFactory_;
  // The TaskRunner to which the getting discounts background tasks are posted.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  // This is used to create a CartDiscountFetcher to fetch discounts.
  std::unique_ptr<CartDiscountFetcherFactory> fetcher_factory_;

  // Posts the discount fetching work to another thread as a delayed background
  // task. This method is expected to run in the UI thread.
  void PostDiscountFetchTask(int delayTime);
  // Performs the actual fetching work.
  static void DoWorkInBackground(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory,
      std::unique_ptr<CartDiscountFetcher> fetcher,
      ContinueToWorkCallback continue_to_work_callback);
  // A callback to handle the fetching result. It also posts
  // #PostDiscountFetchTask to the UI thread to continue working.
  static void OnWorkFinished(ContinueToWorkCallback continue_to_work_callback);

  base::WeakPtrFactory<FetchDiscountWorker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CART_FETCH_DISCOUNT_WORKER_H_
