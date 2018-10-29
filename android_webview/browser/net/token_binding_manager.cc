// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/token_binding_manager.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/callback_helpers.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace android_webview {

using content::BrowserThread;
using net::ChannelIDService;
using net::ChannelIDStore;

namespace {

void CompletionCallback(TokenBindingManager::KeyReadyCallback callback,
                        ChannelIDService::Request* request,
                        std::unique_ptr<crypto::ECPrivateKey>* key,
                        int status) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback), status, base::Owned(key->release())));
}

void DeletionCompleteCallback(
    TokenBindingManager::DeletionCompleteCallback callback) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(callback));
}

void GetKeyImpl(const std::string& host,
                TokenBindingManager::KeyReadyCallback callback,
                scoped_refptr<net::URLRequestContextGetter> context_getter) {
  ChannelIDService* service =
      context_getter->GetURLRequestContext()->channel_id_service();
  ChannelIDService::Request* request = new ChannelIDService::Request();
  std::unique_ptr<crypto::ECPrivateKey>* key =
      new std::unique_ptr<crypto::ECPrivateKey>();
  // The request will own the callback if the call to service returns
  // PENDING. The request releases the ownership before calling the callback.
  // TODO(crbug.com/714018): Update base::Bind here to BindOnce after we update
  // net::CompletionCallback to support OnceCallback.
  net::CompletionCallback completion_callback =
      base::Bind(&CompletionCallback, base::Passed(&callback),
                 base::Owned(request), base::Owned(key));
  int status =
      service->GetOrCreateChannelID(host, key, completion_callback, request);
  if (status == net::ERR_IO_PENDING) {
    // The operation is pending, callback will be called async.
    return;
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(completion_callback, status));
}

void DeleteKeyImpl(const std::string& host,
                   TokenBindingManager::DeletionCompleteCallback callback,
                   scoped_refptr<net::URLRequestContextGetter> context_getter,
                   bool all) {
  ChannelIDService* service =
      context_getter->GetURLRequestContext()->channel_id_service();
  ChannelIDStore* store = service->GetChannelIDStore();
  base::OnceClosure completion_callback =
      base::BindOnce(&DeletionCompleteCallback, std::move(callback));
  if (all) {
    store->DeleteAll(
        base::AdaptCallbackForRepeating(std::move(completion_callback)));
  } else {
    store->DeleteChannelID(
        host, base::AdaptCallbackForRepeating(std::move(completion_callback)));
  }
}

}  // namespace

base::LazyInstance<TokenBindingManager>::Leaky g_lazy_instance;

TokenBindingManager* TokenBindingManager::GetInstance() {
  return g_lazy_instance.Pointer();
}

TokenBindingManager::TokenBindingManager() : enabled_(false) {}

void TokenBindingManager::GetKey(const std::string& host,
                                 KeyReadyCallback callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      content::BrowserContext::GetDefaultStoragePartition(
          AwBrowserContext::GetDefault())->GetURLRequestContext();
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetKeyImpl, host, std::move(callback), context_getter));
}

void TokenBindingManager::DeleteKey(const std::string& host,
                                    DeletionCompleteCallback callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      content::BrowserContext::GetDefaultStoragePartition(
          AwBrowserContext::GetDefault())->GetURLRequestContext();
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DeleteKeyImpl, host, std::move(callback), context_getter,
                     false));
}

void TokenBindingManager::DeleteAllKeys(DeletionCompleteCallback callback) {
  scoped_refptr<net::URLRequestContextGetter> context_getter =
      content::BrowserContext::GetDefaultStoragePartition(
          AwBrowserContext::GetDefault())->GetURLRequestContext();
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&DeleteKeyImpl, "", std::move(callback), context_getter,
                     true));
}

}  // namespace android_webview
