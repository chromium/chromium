// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_TAB_HELPER_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_TAB_HELPER_H_

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"

class StorageAccessAPIService;

class StorageAccessAPITabHelper : public content::WebContentsObserver {
 public:
  // `service` must be non-null and must outlive `this`.
  StorageAccessAPITabHelper(content::WebContents* web_contents,
                            StorageAccessAPIService* service);
  StorageAccessAPITabHelper(const StorageAccessAPITabHelper&) = delete;
  StorageAccessAPITabHelper& operator=(const StorageAccessAPITabHelper&) =
      delete;
  ~StorageAccessAPITabHelper() override;

  // WebContentsObserver:
  void FrameReceivedUserActivation(content::RenderFrameHost* rfh) override;

 private:
  raw_ref<StorageAccessAPIService> service_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_API_TAB_HELPER_H_
