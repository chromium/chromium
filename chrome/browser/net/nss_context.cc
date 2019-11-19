// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"

using content::BrowserThread;

namespace {

// Relays callback to the right message loop.
void DidGetCertDBOnIOThread(
    const scoped_refptr<base::SequencedTaskRunner>& response_task_runner,
    const base::Callback<void(net::NSSCertDatabase*)>& callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_task_runner->PostTask(FROM_HERE, base::BindOnce(callback, cert_db));
}

// Gets NSSCertDatabase for the resource context.
void GetCertDBOnIOThread(
    content::ResourceContext* context,
    const scoped_refptr<base::SequencedTaskRunner>& response_task_runner,
    const base::Callback<void(net::NSSCertDatabase*)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Note that the callback will be used only if the cert database hasn't yet
  // been initialized.
  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      context,
      base::Bind(&DidGetCertDBOnIOThread, response_task_runner, callback));

  if (cert_db)
    DidGetCertDBOnIOThread(response_task_runner, callback, cert_db);
}

}  // namespace

void GetNSSCertDatabaseForProfile(
    Profile* profile,
    const base::Callback<void(net::NSSCertDatabase*)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetCertDBOnIOThread, profile->GetResourceContext(),
                     base::ThreadTaskRunnerHandle::Get(), callback));
}

