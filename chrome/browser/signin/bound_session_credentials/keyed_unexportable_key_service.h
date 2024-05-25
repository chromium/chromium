// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_KEYED_UNEXPORTABLE_KEY_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_KEYED_UNEXPORTABLE_KEY_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/unexportable_keys/unexportable_key_service.h"

class KeyedUnexportableKeyService
    : public KeyedService,
      public unexportable_keys::UnexportableKeyService {};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_KEYED_UNEXPORTABLE_KEY_SERVICE_H_
