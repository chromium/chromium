// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_
#define CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_

#include <memory>

#include "content/public/browser/dips_delegate.h"

namespace content {
class BrowserContext;
}

class ChromeContentBrowserClient;

class ChromeDipsDelegate : public content::DipsDelegate {
 public:
  // TODO(rtarpine): remove this and make clients call
  // ContentBrowserClient::CreateDipsDelegate(), falling back on a default
  // implementation if it returned null, once DIPS has moved to //content.
  static std::unique_ptr<content::DipsDelegate> Create();

  bool ShouldEnableDips(content::BrowserContext* browser_context) override;

 private:
  friend class ChromeContentBrowserClient;
};

#endif  // CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_
