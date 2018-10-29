// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin/merge_session_xhr_request_waiter.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager.h"
#include "chrome/browser/chromeos/login/signin/oauth2_login_manager_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Maximum time for delaying XHR requests.
const int kMaxRequestWaitTimeMS = 10000;

}  // namespace

MergeSessionXHRRequestWaiter::MergeSessionXHRRequestWaiter(
    Profile* profile,
    const merge_session_throttling_utils::CompletionCallback& callback)
    : profile_(profile), callback_(callback), weak_ptr_factory_(this) {}

MergeSessionXHRRequestWaiter::~MergeSessionXHRRequestWaiter() {
  chromeos::OAuth2LoginManager* manager =
      chromeos::OAuth2LoginManagerFactory::GetInstance()->GetForProfile(
          profile_);
  if (manager)
    manager->RemoveObserver(this);
}

void MergeSessionXHRRequestWaiter::StartWaiting() {
  OAuth2LoginManager* manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile_);
  if (manager && manager->ShouldBlockTabLoading()) {
    DVLOG(1) << "Waiting for XHR request throttle";
    manager->AddObserver(this);
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&MergeSessionXHRRequestWaiter::OnTimeout,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kMaxRequestWaitTimeMS));
  } else {
    NotifyBlockingDone();
  }
}

void MergeSessionXHRRequestWaiter::OnSessionRestoreStateChanged(
    Profile* user_profile,
    OAuth2LoginManager::SessionRestoreState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  OAuth2LoginManager* manager =
      OAuth2LoginManagerFactory::GetInstance()->GetForProfile(profile_);
  DVLOG(1) << "Merge session throttle should "
           << (!manager->ShouldBlockTabLoading() ? " NOT" : "")
           << " be blocking now, " << state;
  if (!manager->ShouldBlockTabLoading()) {
    DVLOG(1) << "Unblocking XHR request throttle due to session merge";
    manager->RemoveObserver(this);
    NotifyBlockingDone();
  }
}

void MergeSessionXHRRequestWaiter::OnTimeout() {
  DVLOG(1) << "Unblocking XHR request throttle due to timeout";
  NotifyBlockingDone();
}

void MergeSessionXHRRequestWaiter::NotifyBlockingDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!callback_.is_null()) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO}, callback_);
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

}  // namespace chromeos
