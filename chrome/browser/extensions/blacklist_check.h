// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLACKLIST_CHECK_H_
#define CHROME_BROWSER_EXTENSIONS_BLACKLIST_CHECK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/blacklist_state.h"
#include "extensions/browser/preload_check.h"

namespace extensions {

class Blacklist;
class Extension;

// Asynchronously checks whether the extension is blacklisted.
class BlacklistCheck : public PreloadCheck {
 public:
  BlacklistCheck(Blacklist* blacklist,
                 scoped_refptr<const Extension> extension);
  ~BlacklistCheck() override;

  // PreloadCheck:
  void Start(ResultCallback callback) override;

 private:
  void OnBlacklistedStateRetrieved(BlacklistState blacklist_state);

  Blacklist* blacklist_;
  ResultCallback callback_;
  base::WeakPtrFactory<BlacklistCheck> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BlacklistCheck);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLACKLIST_CHECK_H_
