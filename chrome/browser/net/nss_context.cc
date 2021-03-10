// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Relays callback to the right message loop.
void DidGetCertDBOnIOThread(
    const scoped_refptr<base::SequencedTaskRunner>& response_task_runner,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), cert_db));
}

// Gets NSSCertDatabase for the resource context.
void GetCertDBOnIOThread(
    NssCertDatabaseGetter database_getter,
    scoped_refptr<base::SequencedTaskRunner> response_task_runner,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Note that the callback will be used only if the cert database hasn't yet
  // been initialized.
  auto completion_callback = base::AdaptCallbackForRepeating(base::BindOnce(
      &DidGetCertDBOnIOThread, response_task_runner, std::move(callback)));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(completion_callback);

  if (cert_db)
    completion_callback.Run(cert_db);
}

}  // namespace

void GetNSSCertDatabaseForProfile(
    Profile* profile,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetCertDBOnIOThread, CreateNSSCertDatabaseGetter(profile),
                     base::ThreadTaskRunnerHandle::Get(), std::move(callback)));
}
