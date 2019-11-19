// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/downloads/offline_page_share_helper.h"

#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/download/android/download_utils.h"
#include "chrome/browser/offline_pages/offline_page_mhtml_archiver.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/page_criteria.h"

using OfflineItemShareInfo = offline_items_collection::OfflineItemShareInfo;

namespace offline_pages {
namespace {

content::WebContents* EmptyWebContentGetter() {
  return nullptr;
}

// Create share info with a content URI or file URI. We try best effort to get
// content URI if |file_path| is in Android public directory. Then try to
// fall back to file URI. If both failed, we will share the URL of the page
// when share info is null or has empty URI.
std::unique_ptr<OfflineItemShareInfo> CreateShareInfo(
    const base::FilePath& file_path) {
  auto share_info = std::make_unique<OfflineItemShareInfo>();
  share_info->uri = DownloadUtils::GetUriStringForPath(file_path);
  return share_info;
}

}  // namespace

OfflinePageShareHelper::OfflinePageShareHelper(OfflinePageModel* model)
    : model_(model) {}

OfflinePageShareHelper::~OfflinePageShareHelper() = default;

void OfflinePageShareHelper::GetShareInfo(const ContentId& id,
                                          ResultCallback result_cb) {
  content_id_ = id;
  result_cb_ = std::move(result_cb);

  PageCriteria criteria;
  criteria.guid = content_id_.id;
  criteria.maximum_matches = 1;
  model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&OfflinePageShareHelper::OnPageGetForShare,
                               weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageShareHelper::OnPageGetForShare(
    const std::vector<OfflinePageItem>& pages) {
  if (pages.empty()) {
    // Continue to share without share info.
    NotifyCompletion(ShareResult::kSuccess, nullptr);
    return;
  }
  const OfflinePageItem& page = pages[0];
  const bool is_suggested = GetPolicy(page.client_id.name_space).is_suggested;
  const bool in_private_dir = model_->IsArchiveInInternalDir(page.file_path);

  // Need to publish internal page to public directory to share the file with
  // content URI instead of the web page URL.
  if (!is_suggested && in_private_dir) {
    AcquireFileAccessPermission();
    return;
  }

  // Try to share the mhtml file if the page is in public directory.
  NotifyCompletion(ShareResult::kSuccess, CreateShareInfo(page.file_path));
}

void OfflinePageShareHelper::AcquireFileAccessPermission() {
  DownloadControllerBase::Get()->AcquireFileAccessPermission(
      base::BindRepeating(&EmptyWebContentGetter),
      base::BindOnce(&OfflinePageShareHelper::OnFileAccessPermissionDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageShareHelper::OnFileAccessPermissionDone(bool granted) {
  if (!granted) {
    NotifyCompletion(ShareResult::kFileAccessPermissionDenied, nullptr);
    return;
  }

  // Retrieve the offline page again in case it's deleted.
  PageCriteria criteria;
  criteria.guid = content_id_.id;
  criteria.maximum_matches = 1;
  model_->GetPagesWithCriteria(
      criteria, base::BindOnce(&OfflinePageShareHelper::OnPageGetForPublish,
                               weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageShareHelper::OnPageGetForPublish(
    const std::vector<OfflinePageItem>& pages) {
  if (pages.empty())
    return;
  // Publish the page.
  model_->PublishInternalArchive(
      pages[0], base::BindOnce(&OfflinePageShareHelper::OnPagePublished,
                               weak_ptr_factory_.GetWeakPtr()));
}

void OfflinePageShareHelper::OnPagePublished(const base::FilePath& file_path,
                                             SavePageResult result) {
  // Get the content URI after the page is published.
  NotifyCompletion(ShareResult::kSuccess, CreateShareInfo(file_path));
}

void OfflinePageShareHelper::NotifyCompletion(
    ShareResult result,
    std::unique_ptr<OfflineItemShareInfo> share_info) {
  DCHECK(result_cb_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_cb_), result, content_id_,
                                std::move(share_info)));
}

}  // namespace offline_pages
