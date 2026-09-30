// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GLIC_GEMINI_ENTERPRISE_MANAGER_H_
#define CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GLIC_GEMINI_ENTERPRISE_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/gemini_enterprise/geic_managed_tab.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"
#include "url/origin.h"

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
  void OpenAuthTab(mojom::OpenAuthTabOptionsPtr options,
                   OpenAuthTabCallback callback) override;
  void CloseAuthTab(mojom::CloseAuthTabOptionsPtr options,
                    CloseAuthTabCallback callback) override;

 private:
  GlicGeminiEnterpriseManager(Profile* profile,
                              const url::Origin& guest_origin);

  // Shared implementation of `OpenAuthTab` (and the deprecated
  // `OpenSignInTab`). Records metrics.
  mojom::OpenAuthTabResponsePtr OpenAuthTabImpl(mojom::AuthTabPurpose purpose,
                                                const std::optional<GURL>& url);
  // Shared implementation of `CloseAuthTab` (and the deprecated
  // `CloseSignInTab`). Records metrics.
  mojom::CloseAuthTabResponsePtr CloseAuthTabImpl(
      mojom::AuthTabPurpose purpose);

  // Returns the tracked tab for `purpose`, or null if `purpose` is invalid.
  GeicManagedTab* GetAuthTab(mojom::AuthTabPurpose purpose);

  bool IsAuthTabURLAllowed(mojom::AuthTabPurpose purpose, const GURL& url);

  raw_ptr<Profile> profile_ = nullptr;
  GeicManagedTab signin_tab_;
  GeicManagedTab connector_oauth_tab_;

  mojo::Receiver<mojom::GeminiEnterpriseHandler> receiver_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GEMINI_ENTERPRISE_GLIC_GEMINI_ENTERPRISE_MANAGER_H_
