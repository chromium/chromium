// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ash/browser_delegate/browser_delegate.h"

class Browser;

namespace ash {

class BrowserDelegateImpl : public BrowserDelegate {
 public:
  explicit BrowserDelegateImpl(Browser* browser);
  virtual ~BrowserDelegateImpl();

  // BrowserDelegate:
  Browser& GetBrowser() const override;
  SessionID GetSessionID() const override;
  content::WebContents* GetActiveWebContents() const override;
  aura::Window* GetNativeWindow() const override;
  bool IsClosing() const override;
  void Show() override;

 private:
  const raw_ref<Browser> browser_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_
