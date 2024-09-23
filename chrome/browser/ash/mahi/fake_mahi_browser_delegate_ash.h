// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_FAKE_MAHI_BROWSER_DELEGATE_ASH_H_
#define CHROME_BROWSER_ASH_MAHI_FAKE_MAHI_BROWSER_DELEGATE_ASH_H_

#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
namespace ash {

class FakeMahiBrowserDelegateAsh : public MahiBrowserDelegateAsh {
 public:
  FakeMahiBrowserDelegateAsh();
  FakeMahiBrowserDelegateAsh(const FakeMahiBrowserDelegateAsh&) = delete;
  FakeMahiBrowserDelegateAsh& operator=(const FakeMahiBrowserDelegateAsh&) =
      delete;
  ~FakeMahiBrowserDelegateAsh() override;

  void GetContentFromClient(
      const base::UnguessableToken& client_id,
      const base::UnguessableToken& page_id,
      crosapi::mojom::MahiBrowserClient::GetContentCallback callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_FAKE_MAHI_BROWSER_DELEGATE_ASH_H_
