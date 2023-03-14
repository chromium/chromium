// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace ukm {
class UkmBackgroundRecorderService;
}  // namespace ukm

namespace url {
class Origin;
}  // namespace url

class ContentIndexMetrics {
 public:
  explicit ContentIndexMetrics(
      ukm::UkmBackgroundRecorderService* ukm_background_service);

  ContentIndexMetrics(const ContentIndexMetrics&) = delete;
  ContentIndexMetrics& operator=(const ContentIndexMetrics&) = delete;

  ~ContentIndexMetrics();

  // Records the category of the Content Index entry after registration.
  void RecordContentAdded(const url::Origin& origin,
                          blink::mojom::ContentCategory category);

  // Records the category of the Content Index entry when a user opens it.
  void RecordContentOpened(content::WebContents* web_contents,
                           blink::mojom::ContentCategory category);

  // Records when a Content Index entry is deleted by a user.
  void RecordContentDeletedByUser(const url::Origin& origin);

 private:
  raw_ptr<ukm::UkmBackgroundRecorderService> ukm_background_service_;
};

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_
