// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_
#define CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_

#include <stdint.h>

#include "base/types/pass_key.h"
#include "content/public/browser/dips_delegate.h"

class ChromeContentBrowserClient;

class ChromeDipsDelegate : public content::DipsDelegate {
 public:
  explicit ChromeDipsDelegate(base::PassKey<ChromeContentBrowserClient>);

  bool ShouldDeleteInteractionRecords(uint64_t remove_mask) override;
};

#endif  // CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_
