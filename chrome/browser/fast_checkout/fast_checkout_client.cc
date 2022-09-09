// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client.h"

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

// static
FastCheckoutClient* FastCheckoutClient::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  FastCheckoutClientImpl::CreateForWebContents(web_contents);
  return FastCheckoutClientImpl::FromWebContents(web_contents);
}
