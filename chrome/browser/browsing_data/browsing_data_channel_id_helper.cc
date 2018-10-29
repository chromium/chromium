// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_channel_id_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/ssl/channel_id_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

class BrowsingDataChannelIDHelperImpl
    : public BrowsingDataChannelIDHelper {
 public:
  explicit BrowsingDataChannelIDHelperImpl(
      net::URLRequestContextGetter* request_context);

  // BrowsingDataChannelIDHelper methods.
  void StartFetching(const FetchResultCallback& callback) override;
  void DeleteChannelID(const std::string& server_id) override;

 private:
  ~BrowsingDataChannelIDHelperImpl() override;

  // Fetch the certs. This must be called in the IO thread.
  void FetchOnIOThread(const FetchResultCallback& callback);

  void OnFetchComplete(
      const FetchResultCallback& callback,
      const net::ChannelIDStore::ChannelIDList& channel_id_list);

  // Delete a single cert. This must be called in IO thread.
  void DeleteOnIOThread(const std::string& server_id);

  // Called when deletion is done.
  void DeleteCallback();

  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataChannelIDHelperImpl);
};

BrowsingDataChannelIDHelperImpl::BrowsingDataChannelIDHelperImpl(
    net::URLRequestContextGetter* request_context)
    : request_context_getter_(request_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BrowsingDataChannelIDHelperImpl::
~BrowsingDataChannelIDHelperImpl() {
}

void BrowsingDataChannelIDHelperImpl::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataChannelIDHelperImpl::FetchOnIOThread, this,
                     callback));
}

void BrowsingDataChannelIDHelperImpl::DeleteChannelID(
    const std::string& server_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataChannelIDHelperImpl::DeleteOnIOThread, this,
                     server_id));
}

void BrowsingDataChannelIDHelperImpl::FetchOnIOThread(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  net::ChannelIDService* channel_id_service =
      request_context_getter_->GetURLRequestContext()->channel_id_service();
  net::ChannelIDStore* cert_store = nullptr;
  if (channel_id_service) {
    cert_store = channel_id_service->GetChannelIDStore();
  }
  if (cert_store) {
    cert_store->GetAllChannelIDs(base::Bind(
        &BrowsingDataChannelIDHelperImpl::OnFetchComplete, this, callback));
  } else {
    OnFetchComplete(callback, net::ChannelIDStore::ChannelIDList());
  }
}

void BrowsingDataChannelIDHelperImpl::OnFetchComplete(
    const FetchResultCallback& callback,
    const net::ChannelIDStore::ChannelIDList& channel_id_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, channel_id_list));
}

void BrowsingDataChannelIDHelperImpl::DeleteOnIOThread(
    const std::string& server_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDStore* cert_store =
      request_context_getter_->GetURLRequestContext()->
      channel_id_service()->GetChannelIDStore();
  if (cert_store) {
    cert_store->DeleteChannelID(
        server_id,
        base::Bind(&BrowsingDataChannelIDHelperImpl::DeleteCallback,
                   this));
  }
}

void BrowsingDataChannelIDHelperImpl::DeleteCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Need to close open SSL connections which may be using the channel ids we
  // are deleting.
  // TODO(mattm): http://crbug.com/166069 Make the server bound cert
  // service/store have observers that can notify relevant things directly.
  request_context_getter_->GetURLRequestContext()->ssl_config_service()->
      NotifySSLConfigChange();
}

}  // namespace

// static
BrowsingDataChannelIDHelper* BrowsingDataChannelIDHelper::Create(
    net::URLRequestContextGetter* request_context) {
  return new BrowsingDataChannelIDHelperImpl(request_context);
}

CannedBrowsingDataChannelIDHelper::
CannedBrowsingDataChannelIDHelper() {}

CannedBrowsingDataChannelIDHelper::
~CannedBrowsingDataChannelIDHelper() {}

void CannedBrowsingDataChannelIDHelper::AddChannelID(
    const net::ChannelIDStore::ChannelID& channel_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  channel_id_map_[channel_id.server_identifier()] =
      channel_id;
}

void CannedBrowsingDataChannelIDHelper::Reset() {
  channel_id_map_.clear();
}

bool CannedBrowsingDataChannelIDHelper::empty() const {
  return channel_id_map_.empty();
}

size_t CannedBrowsingDataChannelIDHelper::GetChannelIDCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return channel_id_map_.size();
}

void CannedBrowsingDataChannelIDHelper::StartFetching(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback.is_null())
    return;
  // We post a task to emulate async fetching behavior.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&CannedBrowsingDataChannelIDHelper::FinishFetching, this,
                     callback));
}

void CannedBrowsingDataChannelIDHelper::FinishFetching(
    const FetchResultCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  net::ChannelIDStore::ChannelIDList channel_id_list;
  for (const auto& pair : channel_id_map_)
    channel_id_list.push_back(pair.second);
  callback.Run(channel_id_list);
}

void CannedBrowsingDataChannelIDHelper::DeleteChannelID(
    const std::string& server_id) {
  NOTREACHED();
}
