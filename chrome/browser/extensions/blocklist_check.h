// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOCKLIST_CHECK_H_
#define CHROME_BROWSER_EXTENSIONS_BLOCKLIST_CHECK_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/preload_check.h"

namespace extensions {

class Blocklist;
class Extension;

// Asynchronously checks whether the extension is blocklisted.
class BlocklistCheck : public PreloadCheck {
 public:
  BlocklistCheck(Blocklist* blocklist,
                 scoped_refptr<const Extension> extension);

  BlocklistCheck(const BlocklistCheck&) = delete;
  BlocklistCheck& operator=(const BlocklistCheck&) = delete;

  ~BlocklistCheck() override;

  // PreloadCheck:
  void Start(ResultCallback callback) override;

 private:
  void OnBlocklistedStateRetrieved(BlocklistState blocklist_state);

  raw_ptr<Blocklist, DanglingUntriaged> blocklist_;
  ResultCallback callback_;
  base::WeakPtrFactory<BlocklistCheck> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BLOCKLIST_CHECK_H_
