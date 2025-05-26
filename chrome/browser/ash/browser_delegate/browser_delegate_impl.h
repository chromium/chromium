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
  BrowserType GetType() const override;
  SessionID GetSessionID() const override;
  bool IsOffTheRecord() const override;
  gfx::Rect GetBounds() const override;
  content::WebContents* GetActiveWebContents() const override;
  size_t GetWebContentsCount() const override;
  content::WebContents* GetWebContentsAt(size_t index) const override;
  aura::Window* GetNativeWindow() const override;
  bool IsClosing() const override;
  void Show() override;
  void Minimize() override;
  void Close() override;
  void AddTab(const GURL& url,
              std::optional<size_t> index,
              TabDisposition disposition) override;
  content::WebContents* NavigateWebApp(const GURL& url,
                                       TabPinning pin_tab) override;

 private:
  const raw_ref<Browser> browser_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_BROWSER_DELEGATE_IMPL_H_
