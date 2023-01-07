// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client.h"

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

// static
ApcClient* ApcClient::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  ApcClientImpl::CreateForWebContents(web_contents);
  return ApcClientImpl::FromWebContents(web_contents);
}
