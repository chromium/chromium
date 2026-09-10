// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GLIC_GEMINI_ENTERPRISE_MANAGER_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GLIC_GEMINI_ENTERPRISE_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"
#include "url/origin.h"

class BrowserWindowInterface;
class Profile;

namespace glic {

// Manages Gemini Enterprise functionality for Glic.
class GlicGeminiEnterpriseManager : public mojom::GeminiEnterpriseHandler {
 public:
  explicit GlicGeminiEnterpriseManager(Profile* profile);
  GlicGeminiEnterpriseManager(const GlicGeminiEnterpriseManager&) = delete;
  GlicGeminiEnterpriseManager& operator=(const GlicGeminiEnterpriseManager&) =
      delete;
  ~GlicGeminiEnterpriseManager() override;

  void Bind(mojo::PendingReceiver<mojom::GeminiEnterpriseHandler> receiver);

  // mojom::GeminiEnterpriseHandler:
  void OpenSignInTab(mojom::OpenSignInTabOptionsPtr options,
                     OpenSignInTabCallback callback) override;
  void CloseSignInTab(mojom::CloseSignInTabOptionsPtr options,
                      CloseSignInTabCallback callback) override;

  bool IsSignInURLAllowedForTesting(const GURL& url) const {
    return IsSignInURLAllowed(url);
  }

 private:
  bool IsSignInURLAllowed(const GURL& url) const;
  BrowserWindowInterface* GetLastActiveBrowserWindowForCurrentProfile() const;

  raw_ptr<Profile> profile_ = nullptr;
  url::Origin gaia_origin_;
  std::optional<url::Origin> guest_origin_;
  tabs::TabHandle tab_active_before_signin_;
  tabs::TabHandle signin_tab_;
  bool has_opened_signin_tab_ = false;

  mojo::Receiver<mojom::GeminiEnterpriseHandler> receiver_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GLIC_GEMINI_ENTERPRISE_MANAGER_H_
