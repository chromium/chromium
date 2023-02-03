// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/nss_cert_database.h"

// Note: This file contains the platform-agnostic components of NssService;
// platform-specific portions are implemented in the _linux.cc and
// _chromeos.cc files.

namespace {

void GetCertDBOnIOThread(
    NssCertDatabaseGetter database_getter,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  if (cert_db)
    std::move(split_callback.second).Run(cert_db);
}

}  // namespace

void NssService::UnsafelyGetNSSCertDatabaseForTesting(
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetCertDBOnIOThread,
                     CreateNSSCertDatabaseGetterForIOThread(),
                     base::BindPostTaskToCurrentDefault(std::move(callback))));
}
