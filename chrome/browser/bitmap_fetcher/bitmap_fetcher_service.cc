// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/image_decoder/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

const size_t kMaxRequests = 25;  // Maximum number of inflight requests allowed.

// Maximum number of cache entries. This was 5 before, which worked well enough
// for few images like weather answers, but with rich entity suggestions showing
// several images at once, even changing some while the user types, a larger
// cache is necessary to avoid flickering. Each cache entry is expected to take
// 16kb (64x64 @ 32bpp).  With 16, the total memory consumed would be ~256kb.
// 16 is double the default number of maximum suggestions so this can
// accommodate one match image plus one answer image for each result.
#if BUILDFLAG(IS_ANDROID)
// Android caches the images in the java layer.
const int kMaxCacheEntries = 0;
#else
const int kMaxCacheEntries = 16;
#endif

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("omnibox_result_change", R"(
        semantics {
          sender: "Omnibox"
          description:
            "Chromium provides answers in the suggestion list for "
            "certain queries that user types in the omnibox. This request "
            "retrieves a small image (for example, an icon illustrating "
            "the current weather conditions) when this can add information "
            "to an answer."
          trigger:
            "Change of results for the query typed by the user in the "
            "omnibox."
          data:
            "The only data sent is the path to an image. No user data is "
            "included, although some might be inferrable (e.g. whether the "
            "weather is sunny or rainy in the user's current location) "
            "from the name of the image in the path."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "You can enable or disable this feature via 'Use a prediction "
            "service to help complete searches and URLs typed in the "
            "address bar.' in Chromium's settings under Advanced. The "
            "feature is enabled by default."
          chrome_policy {
            SearchSuggestEnabled {
                policy_options {mode: MANDATORY}
                SearchSuggestEnabled: false
            }
          }
        })");

}  // namespace.

class BitmapFetcherRequest {
 public:
  BitmapFetcherRequest(BitmapFetcherService::RequestId request_id,
                       BitmapFetcherService::BitmapFetchedCallback callback);

  BitmapFetcherRequest(const BitmapFetcherRequest&) = delete;
  BitmapFetcherRequest& operator=(const BitmapFetcherRequest&) = delete;

  ~BitmapFetcherRequest();

  void NotifyImageChanged(const SkBitmap* bitmap);
  BitmapFetcherService::RequestId request_id() const { return request_id_; }

  // Weak ptr |fetcher| is used to identify associated fetchers.
  void set_fetcher(const BitmapFetcher* fetcher) { fetcher_ = fetcher; }
  const BitmapFetcher* get_fetcher() const { return fetcher_; }

 private:
  const BitmapFetcherService::RequestId request_id_;
  BitmapFetcherService::BitmapFetchedCallback callback_;
  raw_ptr<const BitmapFetcher> fetcher_;
};

BitmapFetcherRequest::BitmapFetcherRequest(
    BitmapFetcherService::RequestId request_id,
    BitmapFetcherService::BitmapFetchedCallback callback)
    : request_id_(request_id), callback_(std::move(callback)) {}

BitmapFetcherRequest::~BitmapFetcherRequest() = default;

void BitmapFetcherRequest::NotifyImageChanged(const SkBitmap* bitmap) {
  if (bitmap && !bitmap->empty())
    std::move(callback_).Run(*bitmap);
}

BitmapFetcherService::CacheEntry::CacheEntry() = default;

BitmapFetcherService::CacheEntry::~CacheEntry() = default;

BitmapFetcherService::BitmapFetcherService(content::BrowserContext* context)
    : shared_data_decoder_(
          std::make_unique<data_decoder::DataDecoder>(base::Seconds(405))),
      cache_(kMaxCacheEntries),
      current_request_id_(1),
      context_(context) {}

BitmapFetcherService::~BitmapFetcherService() {
  // |active_fetchers_|'s elements must be destructured before
  // |shared_data_decoder_|, as the former contain unowned pointers to the
  // latter.
  requests_.clear();
  active_fetchers_.clear();
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

BitmapFetcherService::RequestId BitmapFetcherService::RequestImageForTesting(
    const GURL& url,
    BitmapFetchedCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  return RequestImageImpl(url, std::move(callback), traffic_annotation);
}

BitmapFetcherService::RequestId BitmapFetcherService::RequestImageImpl(
    const GURL& url,
    BitmapFetchedCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // Reject invalid URLs and limit number of simultaneous in-flight requests.
  if (!url.is_valid() || requests_.size() > kMaxRequests) {
    return REQUEST_ID_INVALID;
  }

  // Create a new request, assigning next available request ID.
  ++current_request_id_;
  if (current_request_id_ == REQUEST_ID_INVALID)
    ++current_request_id_;
  int request_id = current_request_id_;
  auto request =
      std::make_unique<BitmapFetcherRequest>(request_id, std::move(callback));

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

void BitmapFetcherService::Prefetch(const GURL& url) {
  if (url.is_valid() && !IsCached(url))
    EnsureFetcherForUrl(url, kTrafficAnnotation);
}

bool BitmapFetcherService::IsCached(const GURL& url) {
  return cache_.Get(url) != cache_.end();
}

std::unique_ptr<BitmapFetcher> BitmapFetcherService::CreateFetcher(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  std::unique_ptr<BitmapFetcher> new_fetcher = std::make_unique<BitmapFetcher>(
      url, this, traffic_annotation, shared_data_decoder_.get());

  new_fetcher->Init(
      net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      network::mojom::CredentialsMode::kInclude);
  new_fetcher->Start(context_->GetDefaultStoragePartition()
                         ->GetURLLoaderFactoryForBrowserProcess()
                         .get());
  return new_fetcher;
}

BitmapFetcherService::RequestId BitmapFetcherService::RequestImage(
    const GURL& url,
    BitmapFetchedCallback callback) {
  return RequestImageImpl(url, std::move(callback), kTrafficAnnotation);
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
  CHECK(it != active_fetchers_.end(), base::NotFatalUntil::M130);
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
    entry->bitmap = std::make_unique<SkBitmap>(*bitmap);
    cache_.Put(fetcher->url(), std::move(entry));
  }

  RemoveFetcher(fetcher);
}
