// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_H_
#define CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_H_

#include "base/callback.h"
#include "base/callback_list.h"

class CertDbInitializer {
 public:
  using ReadyCallback = base::OnceCallback<void(bool is_success)>;

  virtual ~CertDbInitializer() = default;

  // Registers `callback` to be notified once initialization is complete.
  // If initialization has already been completed, `callback` will be
  // synchronously invoked and an empty subscription returned; otherwise,
  // `callback` will be invoked when initialization is completed, as long
  // as the subscription is still live.
  virtual base::CallbackListSubscription WaitUntilReady(
      ReadyCallback callback) = 0;
};

#endif  // CHROME_BROWSER_LACROS_CERT_DB_INITIALIZER_H_
