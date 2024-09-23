// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/fake_mahi_browser_delegate_ash.h"

#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
namespace ash {

FakeMahiBrowserDelegateAsh::FakeMahiBrowserDelegateAsh() = default;
FakeMahiBrowserDelegateAsh::~FakeMahiBrowserDelegateAsh() = default;

void FakeMahiBrowserDelegateAsh::GetContentFromClient(
    const base::UnguessableToken& client_id,
    const base::UnguessableToken& page_id,
    crosapi::mojom::MahiBrowserClient::GetContentCallback callback) {
  std::move(callback).Run(crosapi::mojom::MahiPageContent::New(
      client_id, page_id, u"Test page content"));
}

}  // namespace ash
