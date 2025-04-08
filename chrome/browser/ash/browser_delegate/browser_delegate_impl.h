// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"

class Browser;

namespace ash {

class BrowserDelegateImpl : public BrowserDelegate {
 public:
  explicit BrowserDelegateImpl(Browser* browser);
  virtual ~BrowserDelegateImpl();

  // BrowserDelegate:
  SessionID GetSessionID() const override;

 private:
  raw_ptr<Browser> const browser_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_
