// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_login_delegate.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/android/jni_android.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"

using namespace base::android;

using content::BrowserThread;
using content::WebContents;

namespace android_webview {

// static
scoped_refptr<AwLoginDelegate> AwLoginDelegate::Create(
    net::AuthChallengeInfo* auth_info,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  scoped_refptr<AwLoginDelegate> instance(
      new AwLoginDelegate(auth_info, std::move(auth_required_callback)));
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AwLoginDelegate::HandleHttpAuthRequestOnUIThread,
                     instance, first_auth_attempt, web_contents_getter));
  return instance;
}

AwLoginDelegate::AwLoginDelegate(
    net::AuthChallengeInfo* auth_info,
    LoginAuthRequiredCallback auth_required_callback)
    : auth_info_(auth_info),
      auth_required_callback_(std::move(auth_required_callback)) {}

AwLoginDelegate::~AwLoginDelegate() {
  // The Auth handler holds a ref count back on |this| object, so it should be
  // impossible to reach here while this object still owns an auth handler.
  DCHECK(!aw_http_auth_handler_);
}

void AwLoginDelegate::Proceed(const base::string16& user,
                              const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&AwLoginDelegate::ProceedOnIOThread,
                                          this, user, password));
}

void AwLoginDelegate::Cancel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AwLoginDelegate::CancelOnIOThread, this));
}

void AwLoginDelegate::HandleHttpAuthRequestOnUIThread(
    bool first_auth_attempt,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* web_contents = web_contents_getter.Run();
  aw_http_auth_handler_.reset(
      new AwHttpAuthHandler(this, auth_info_.get(), first_auth_attempt));
  if (!aw_http_auth_handler_->HandleOnUIThread(web_contents)) {
    Cancel();
    return;
  }
}

void AwLoginDelegate::CancelOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!auth_required_callback_.is_null())
    std::move(auth_required_callback_).Run(base::nullopt);
  DeleteAuthHandlerSoon();
}

void AwLoginDelegate::ProceedOnIOThread(const base::string16& user,
                                        const base::string16& password) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!auth_required_callback_.is_null()) {
    std::move(auth_required_callback_)
        .Run(net::AuthCredentials(user, password));
  }
  DeleteAuthHandlerSoon();
}

void AwLoginDelegate::OnRequestCancelled() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auth_required_callback_.Reset();
  DeleteAuthHandlerSoon();
}

void AwLoginDelegate::DeleteAuthHandlerSoon() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&AwLoginDelegate::DeleteAuthHandlerSoon, this));
    return;
  }
  aw_http_auth_handler_.reset();
}

}  // namespace android_webview
