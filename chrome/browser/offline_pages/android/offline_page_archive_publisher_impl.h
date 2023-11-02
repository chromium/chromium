// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_ARCHIVE_PUBLISHER_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_ARCHIVE_PUBLISHER_IMPL_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "components/offline_pages/core/offline_page_archive_publisher.h"
#include "components/offline_pages/core/offline_page_item.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace offline_pages {

class ArchiveManager;

class OfflinePageArchivePublisherImpl : public OfflinePageArchivePublisher {
 public:
  class Delegate {
   public:
    Delegate() = default;

    // Returns true if a system download manager is available on this platform.
    virtual bool IsDownloadManagerInstalled();

    // Adds the archive to downloads.
    virtual PublishArchiveResult AddCompletedDownload(
        const OfflinePageItem& page);

    // Removes pages from downloads, returning the number of pages removed.
    virtual int Remove(
        const std::vector<int64_t>& android_download_manager_ids);

   private:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
  };

  explicit OfflinePageArchivePublisherImpl(ArchiveManager* archive_manager);
  ~OfflinePageArchivePublisherImpl() override;

  void SetDelegateForTesting(Delegate* delegate);

  // OfflinePageArchivePublisher implementation.
  void PublishArchive(
      const OfflinePageItem& offline_page,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      PublishArchiveDoneCallback publish_done_callback) const override;

  void UnpublishArchives(
      const std::vector<PublishedArchiveId>& publish_ids) const override;

  base::WeakPtr<OfflinePageArchivePublisher> GetWeakPtr() override;

 private:
  raw_ptr<ArchiveManager> archive_manager_;
  raw_ptr<Delegate> delegate_;
  base::WeakPtrFactory<OfflinePageArchivePublisher> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_OFFLINE_PAGE_ARCHIVE_PUBLISHER_IMPL_H_
