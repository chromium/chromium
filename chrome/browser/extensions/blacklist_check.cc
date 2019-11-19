// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist_check.h"

#include "base/bind.h"
#include "chrome/browser/extensions/blacklist.h"
#include "extensions/common/extension.h"

namespace extensions {

BlacklistCheck::BlacklistCheck(Blacklist* blacklist,
                               scoped_refptr<const Extension> extension)
    : PreloadCheck(extension), blacklist_(blacklist) {}

BlacklistCheck::~BlacklistCheck() {}

void BlacklistCheck::Start(ResultCallback callback) {
  callback_ = std::move(callback);

  blacklist_->IsBlacklisted(
      extension()->id(),
      base::Bind(&BlacklistCheck::OnBlacklistedStateRetrieved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BlacklistCheck::OnBlacklistedStateRetrieved(
    BlacklistState blacklist_state) {
  Errors errors;
  if (blacklist_state == BlacklistState::BLACKLISTED_MALWARE)
    errors.insert(PreloadCheck::BLACKLISTED_ID);
  else if (blacklist_state == BlacklistState::BLACKLISTED_UNKNOWN)
    errors.insert(PreloadCheck::BLACKLISTED_UNKNOWN);
  std::move(callback_).Run(errors);
}

}  // namespace extensions
