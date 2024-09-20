// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_
#define CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_

#include <memory>

#include "base/types/pass_key.h"
#include "content/public/browser/dips_delegate.h"

class DIPSService;

namespace content {
class BrowserContext;
}

class ChromeDipsDelegate : public content::DipsDelegate {
 public:
  using PassKey = base::PassKey<ChromeDipsDelegate>;

  // The constructor takes a PassKey so that the factory method Create() can
  // call std::make_unique() to create an instance, while other classes cannot.
  explicit ChromeDipsDelegate(PassKey);

  // TODO(rtarpine): remove this and make clients call
  // ContentBrowserClient::CreateDipsDelegate(), falling back on a default
  // implementation if it returned null, once DIPS has moved to //content.
  static std::unique_ptr<content::DipsDelegate> Create();

  bool ShouldEnableDips(content::BrowserContext* browser_context) override;

  void OnDipsServiceCreated(content::BrowserContext* browser_context,
                            DIPSService* dips_service) override;
};

#endif  // CHROME_BROWSER_DIPS_CHROME_DIPS_DELEGATE_H_
