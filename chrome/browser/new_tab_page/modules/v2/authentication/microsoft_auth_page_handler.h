// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_AUTHENTICATION_MICROSOFT_AUTH_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_AUTHENTICATION_MICROSOFT_AUTH_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

class MicrosoftAuthPageHandler
    : public ntp::authentication::mojom::MicrosoftAuthPageHandler {
 public:
  static const char kLastDismissedTimePrefName[];
  static const base::TimeDelta kDismissDuration;

  explicit MicrosoftAuthPageHandler(
      mojo::PendingReceiver<
          ntp::authentication::mojom::MicrosoftAuthPageHandler> handler,
      Profile* profile);
  ~MicrosoftAuthPageHandler() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ntp::authentication::mojom::MicrosoftAuthPageHandler:
  void ShouldShowModule(ShouldShowModuleCallback callback) override;
  void DismissModule() override;
  void RestoreModule() override;

 private:
  mojo::Receiver<ntp::authentication::mojom::MicrosoftAuthPageHandler> handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_AUTHENTICATION_MICROSOFT_AUTH_PAGE_HANDLER_H_
