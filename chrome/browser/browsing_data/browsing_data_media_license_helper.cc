// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_media_license_helper.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using content::BrowserThread;

namespace {

// TODO(crbug.com/1231162): Delete this class. Media license data is now managed
// by the quota system.
// An implementation of the BrowsingDataMediaLicenseHelper interface that
// determine data on media licenses in a given |filesystem_context| and
// returns a list of StorageUsageInfo items to a client.
class BrowsingDataMediaLicenseHelperImpl final
    : public BrowsingDataMediaLicenseHelper {
 public:
  // BrowsingDataMediaLicenseHelper implementation
  explicit BrowsingDataMediaLicenseHelperImpl(
      storage::FileSystemContext* filesystem_context);

  BrowsingDataMediaLicenseHelperImpl(
      const BrowsingDataMediaLicenseHelperImpl&) = delete;
  BrowsingDataMediaLicenseHelperImpl& operator=(
      const BrowsingDataMediaLicenseHelperImpl&) = delete;

  void StartFetching(FetchCallback callback) final;
  void DeleteMediaLicenseOrigin(const url::Origin& origin) final;

 private:
  ~BrowsingDataMediaLicenseHelperImpl() final;

  // Keep a reference to the FileSystemContext object for the current profile
  // for use on the file task runner.
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
};

BrowsingDataMediaLicenseHelperImpl::BrowsingDataMediaLicenseHelperImpl(
    storage::FileSystemContext* filesystem_context)
    : filesystem_context_(filesystem_context) {
  DCHECK(filesystem_context_.get());
}

BrowsingDataMediaLicenseHelperImpl::~BrowsingDataMediaLicenseHelperImpl() {}

void BrowsingDataMediaLicenseHelperImpl::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  // Do nothing, since there is nothing to fetch.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::list<content::StorageUsageInfo>()));
}

void BrowsingDataMediaLicenseHelperImpl::DeleteMediaLicenseOrigin(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Do nothing, since there is nothing to delete.
}

}  // namespace

// static
BrowsingDataMediaLicenseHelper* BrowsingDataMediaLicenseHelper::Create(
    storage::FileSystemContext* filesystem_context) {
  return new BrowsingDataMediaLicenseHelperImpl(filesystem_context);
}
