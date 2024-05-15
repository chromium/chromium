// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTENT_WEB_CONTENTS_FACTORY_DATA_DELETER_H_
#define CHROME_BROWSER_ANDROID_CONTENT_WEB_CONTENTS_FACTORY_DATA_DELETER_H_

#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

// A class that deletes all data in a storage partition for a given
// `storage_partition_config`. This class owns itself and will delete itself
// when `WebContentsDestroyed()` is called.
class WebContentsFactoryDataDeleter : public content::WebContentsObserver {
 public:
  WebContentsFactoryDataDeleter(
      content::WebContents* web_contents,
      content::StoragePartitionConfig storage_partition_config)
      : content::WebContentsObserver(web_contents),
        storage_partition_config_(storage_partition_config) {}

  void WebContentsDestroyed() override;

 private:
  content::StoragePartitionConfig storage_partition_config_;
};

#endif  // CHROME_BROWSER_ANDROID_CONTENT_WEB_CONTENTS_FACTORY_DATA_DELETER_H_
