// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_H_
#define CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace data_decoder {
class DataDecoder;
}  // namespace data_decoder

class BitmapFetcher;
class BitmapFetcherRequest;
class GURL;
class SkBitmap;

// Service to retrieve images for Answers in Suggest.
class BitmapFetcherService : public KeyedService, public BitmapFetcherDelegate {
 public:
  typedef int RequestId;
  static const RequestId REQUEST_ID_INVALID = 0;
  using BitmapFetchedCallback =
      base::OnceCallback<void(const SkBitmap& bitmap)>;

  class Observer {
   public:
    virtual ~Observer() {}

    // Called whenever the image changes. Called with an empty image if the
    // fetch failed or the request ended for any reason.
    // TODO(dschuyler) The comment differs from what the code does, follow-up.
    virtual void OnImageChanged(RequestId request_id,
                                const SkBitmap& answers_image) = 0;
  };

  explicit BitmapFetcherService(content::BrowserContext* context);
  ~BitmapFetcherService() override;

  // Cancels a request, if it is still in-flight.
  void CancelRequest(RequestId requestId);

  // Requests a new image. Will either trigger download or satisfy from cache.
  // If there are too many outstanding requests, the request will fail and
  // |callback| will be called to signal failure. Otherwise, |callback| will be
  // called with either the cached image or the downloaded one.
  // NOTE: The callback might be called back synchronously from RequestImage if
  // the image is already in the cache.
  RequestId RequestImage(const GURL& url, BitmapFetchedCallback callback);

  // Start fetching the image at the given |url|.
  void Prefetch(const GURL& url);

  // Prepare the |shared_data_decoder_| for use. If it has either not been
  // created yet, it will be created; if it is not bound (e.g. due to idle
  // timeout), it will be bound. Calling this is optional, as invoking
  // |RequestImage()| or |Prefetch()| will prepare the |shared_data_decoder_|.
  // This is meant to help reduce latency if a caller knows they will need the
  // decoder ahead of time.
  void WakeupDecoder();

  // Return true if |url| is found in |cache_|. This will move the |url| to the
  // front of the recency list.
  bool IsCached(const GURL& url);

 protected:
  // Create a bitmap fetcher for the given |url| and start it. Virtual method
  // so tests can override this for different behavior.
  virtual std::unique_ptr<BitmapFetcher> CreateFetcher(
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

 private:
  friend class BitmapFetcherServiceTest;

  // Same as the public RequestImage above. Used for testing.
  RequestId RequestImageForTesting(
      const GURL& url,
      BitmapFetchedCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  RequestId RequestImageImpl(
      const GURL& url,
      BitmapFetchedCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Gets the existing fetcher for |url| or constructs a new one if it doesn't
  // exist.
  const BitmapFetcher* EnsureFetcherForUrl(
      const GURL& url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Find a fetcher with a given |url|. Return NULL if none is found.
  const BitmapFetcher* FindFetcherForUrl(const GURL& url);

  // Remove |fetcher| from list of active fetchers. |fetcher| MUST be part of
  // the list.
  void RemoveFetcher(const BitmapFetcher* fetcher);

  // BitmapFetcherDelegate implementation.
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  // The data decoder shared by requests. Using a shared decoder has latency
  // benefits. Can be a nullptr if the appropriate corresponding feature is
  // disabled, in which case, BitmapFetcher will start up an isolated decoder
  // per request.
  std::unique_ptr<data_decoder::DataDecoder> shared_data_decoder_;

  // Currently active image fetchers.
  std::vector<std::unique_ptr<BitmapFetcher>> active_fetchers_;

  // Currently active requests.
  std::vector<std::unique_ptr<BitmapFetcherRequest>> requests_;

  // Cache of retrieved images.
  struct CacheEntry {
    CacheEntry();
    ~CacheEntry();

    std::unique_ptr<const SkBitmap> bitmap;
  };
  base::MRUCache<GURL, std::unique_ptr<CacheEntry>> cache_;

  // Current request ID to be used.
  int current_request_id_;

  // Browser context this service is active for.
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcherService);
};

#endif  // CHROME_BROWSER_BITMAP_FETCHER_BITMAP_FETCHER_SERVICE_H_
