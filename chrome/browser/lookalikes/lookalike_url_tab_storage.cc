// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_tab_storage.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_contents.h"

// This bit of chaos ensures that kAllowlistKey is an arbitrary but
// unique-in-the-process value (namely, its own memory address) without casts.
const void* const kAllowlistKey = &kAllowlistKey;

LookalikeUrlTabStorage::InterstitialParams::InterstitialParams() = default;

LookalikeUrlTabStorage::InterstitialParams::~InterstitialParams() = default;

LookalikeUrlTabStorage::InterstitialParams::InterstitialParams(
    const InterstitialParams& other) = default;

LookalikeUrlTabStorage::LookalikeUrlTabStorage() = default;

LookalikeUrlTabStorage::~LookalikeUrlTabStorage() = default;

// static
LookalikeUrlTabStorage* LookalikeUrlTabStorage::GetOrCreate(
    content::WebContents* web_contents) {
  LookalikeUrlTabStorage* storage = static_cast<LookalikeUrlTabStorage*>(
      web_contents->GetUserData(kAllowlistKey));
  if (!storage) {
    storage = new LookalikeUrlTabStorage;
    web_contents->SetUserData(kAllowlistKey, base::WrapUnique(storage));
  }
  return storage;
}

void LookalikeUrlTabStorage::OnLookalikeInterstitialShown(
    const GURL& url,
    const content::Referrer& referrer,
    const std::vector<GURL>& redirect_chain) {
  interstitial_params_.url = url;
  interstitial_params_.referrer = referrer;
  interstitial_params_.redirect_chain = redirect_chain;
}

void LookalikeUrlTabStorage::ClearInterstitialParams() {
  interstitial_params_.url = GURL();
  interstitial_params_.referrer = content::Referrer();
  interstitial_params_.redirect_chain.clear();
}

LookalikeUrlTabStorage::InterstitialParams
LookalikeUrlTabStorage::GetInterstitialParams() const {
  return interstitial_params_;
}
