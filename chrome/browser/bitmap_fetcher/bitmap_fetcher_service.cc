// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"

#include <stddef.h>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const size_t kMaxRequests = 25;  // Maximum number of inflight requests allowed.

// Maximum number of cache entries. This was 5 before, which worked well enough
// for few images like weather answers, but with rich entity suggestions showing
// several images at once, even changing some while the user types, a larger
// cache is necessary to avoid flickering. Each cache entry is expected to take
// 16kb (64x64 @ 32bpp).  With 12, the total memory consumed would be ~192kb.
// 12 is double the default number of maximum suggestions so this can
// accommodate one match image plus one answer image for each result.
#if defined(OS_ANDROID)
// Android caches the images in the java layer.
const int kMaxCacheEntries = 0;
#else
const int kMaxCacheEntries = 12;
#endif

}  // namespace.

class BitmapFetcherRequest {
 public:
  BitmapFetcherRequest(BitmapFetcherService::RequestId request_id,
                       BitmapFetcherService::Observer* observer);
  ~BitmapFetcherRequest();

  void NotifyImageChanged(const SkBitmap* bitmap);
  BitmapFetcherService::RequestId request_id() const { return request_id_; }

  // Weak ptr |fetcher| is used to identify associated fetchers.
  void set_fetcher(const BitmapFetcher* fetcher) { fetcher_ = fetcher; }
  const BitmapFetcher* get_fetcher() const { return fetcher_; }

 private:
  const BitmapFetcherService::RequestId request_id_;
  std::unique_ptr<BitmapFetcherService::Observer> observer_;
  const BitmapFetcher* fetcher_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcherRequest);
};

BitmapFetcherRequest::BitmapFetcherRequest(
    BitmapFetcherService::RequestId request_id,
    BitmapFetcherService::Observer* observer)
    : request_id_(request_id), observer_(observer) {
}

BitmapFetcherRequest::~BitmapFetcherRequest() {
}

void BitmapFetcherRequest::NotifyImageChanged(const SkBitmap* bitmap) {
  if (bitmap && !bitmap->empty())
    observer_->OnImageChanged(request_id_, *bitmap);
}

BitmapFetcherService::CacheEntry::CacheEntry() {
}

BitmapFetcherService::CacheEntry::~CacheEntry() {
}

BitmapFetcherService::BitmapFetcherService(content::BrowserContext* context)
    : cache_(kMaxCacheEntries), current_request_id_(1), context_(context) {
}

BitmapFetcherService::~BitmapFetcherService() {
}

void BitmapFetcherService::CancelRequest(int request_id) {
  for (auto iter = requests_.begin(); iter != requests_.end(); ++iter) {
    if ((*iter)->request_id() == request_id) {
      requests_.erase(iter);
      // Deliberately leave the associated fetcher running to populate cache.
      return;
    }
  }
}

BitmapFetcherService::RequestId BitmapFetcherService::RequestImage(
    const GURL& url,
    Observer* observer,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // Reject invalid URLs and limit number of simultaneous in-flight requests.
  if (!url.is_valid() || requests_.size() > kMaxRequests) {
    delete observer;
    return REQUEST_ID_INVALID;
  }

  // Create a new request, assigning next available request ID.
  ++current_request_id_;
  if (current_request_id_ == REQUEST_ID_INVALID)
    ++current_request_id_;
  int request_id = current_request_id_;
  auto request = std::make_unique<BitmapFetcherRequest>(request_id, observer);

  // Check for existing images first.
  auto iter = cache_.Get(url);
  if (iter != cache_.end()) {
    BitmapFetcherService::CacheEntry* entry = iter->second.get();
    request->NotifyImageChanged(entry->bitmap.get());

    // There is no request ID associated with this - data is already delivered.
    return REQUEST_ID_INVALID;
  }

  // Make sure there's a fetcher for this URL and attach to request.
  const BitmapFetcher* fetcher = EnsureFetcherForUrl(url, traffic_annotation);
  request->set_fetcher(fetcher);

  requests_.push_back(std::move(request));
  return request_id;
}

void BitmapFetcherService::Prefetch(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (url.is_valid())
    EnsureFetcherForUrl(url, traffic_annotation);
}

std::unique_ptr<BitmapFetcher> BitmapFetcherService::CreateFetcher(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  std::unique_ptr<BitmapFetcher> new_fetcher(
      new BitmapFetcher(url, this, traffic_annotation));

  new_fetcher->Init(
      std::string(),
      net::URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kInclude);
  new_fetcher->Start(
      content::BrowserContext::GetDefaultStoragePartition(context_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
  return new_fetcher;
}

const BitmapFetcher* BitmapFetcherService::EnsureFetcherForUrl(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  const BitmapFetcher* fetcher = FindFetcherForUrl(url);
  if (fetcher)
    return fetcher;

  std::unique_ptr<BitmapFetcher> new_fetcher =
      CreateFetcher(url, traffic_annotation);
  active_fetchers_.push_back(std::move(new_fetcher));
  return active_fetchers_.back().get();
}

const BitmapFetcher* BitmapFetcherService::FindFetcherForUrl(const GURL& url) {
  for (auto it = active_fetchers_.begin(); it != active_fetchers_.end(); ++it) {
    if (url == (*it)->url())
      return it->get();
  }
  return nullptr;
}

void BitmapFetcherService::RemoveFetcher(const BitmapFetcher* fetcher) {
  auto it = active_fetchers_.begin();
  for (; it != active_fetchers_.end(); ++it) {
    if (it->get() == fetcher)
      break;
  }
  // RemoveFetcher should always result in removal.
  DCHECK(it != active_fetchers_.end());
  active_fetchers_.erase(it);
}

void BitmapFetcherService::OnFetchComplete(const GURL& url,
                                           const SkBitmap* bitmap) {
  const BitmapFetcher* fetcher = FindFetcherForUrl(url);
  DCHECK(fetcher);

  // Notify all attached requests of completion.
  auto iter = requests_.begin();
  while (iter != requests_.end()) {
    if ((*iter)->get_fetcher() == fetcher) {
      (*iter)->NotifyImageChanged(bitmap);
      iter = requests_.erase(iter);
    } else {
      ++iter;
    }
  }

  if (bitmap && !bitmap->isNull()) {
    std::unique_ptr<CacheEntry> entry(new CacheEntry);
    entry->bitmap.reset(new SkBitmap(*bitmap));
    cache_.Put(fetcher->url(), std::move(entry));
  }

  RemoveFetcher(fetcher);
}
