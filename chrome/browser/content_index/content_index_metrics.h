// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_

#include "base/macros.h"
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
  ~ContentIndexMetrics();

  // Records the category of the Content Index entry after registration.
  void RecordContentAdded(const url::Origin& origin,
                          blink::mojom::ContentCategory category);

  // Records the category of the Content Index entry when a user opens it.
  void RecordContentOpened(content::WebContents* web_contents,
                           blink::mojom::ContentCategory category);

  // Records when a Content Index entry is deleted by a user.
  void RecordContentDeletedByUser(const url::Origin& origin);

  // Records the number of Content Index entries available when requested.
  static void RecordContentIndexEntries(size_t num_entries);

 private:
  ukm::UkmBackgroundRecorderService* ukm_background_service_;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexMetrics);
};

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_
