// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_H_
#define CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/net/nss_service.h"

class CertDbInitializer {
 public:
  virtual ~CertDbInitializer() = default;

  // Registers `callback` to be notified once initialization is complete (as
  // long as the subscription is still live).
  virtual base::CallbackListSubscription WaitUntilReady(
      base::OnceClosure callback) = 0;

  // Must be called on the UI thread. Returns a Getter that may only be invoked
  // on the IO thread. To avoid UAF, the getter must be immediately posted to
  // the IO thread and then invoked.
  // TODO(crbug.com/40753707): Rework the getter interface.
  virtual NssCertDatabaseGetter CreateNssCertDatabaseGetterForIOThread() = 0;
};

#endif  // CHROME_BROWSER_LACROS_CERT_CERT_DB_INITIALIZER_H_
