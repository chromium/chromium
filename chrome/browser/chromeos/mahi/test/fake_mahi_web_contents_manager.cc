// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"

#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace mahi {

FakeMahiWebContentsManager::FakeMahiWebContentsManager() = default;

FakeMahiWebContentsManager::~FakeMahiWebContentsManager() = default;

gfx::ImageSkia FakeMahiWebContentsManager::GetFavicon(
    content::WebContents* web_contents) const {
  return gfx::test::CreateImage(27, 27).AsImageSkia();
}

void FakeMahiWebContentsManager::RequestContentFromPage(
    const base::UnguessableToken& page_id,
    GetContentCallback callback) {
  // Forwards the request to `client_`, the first place to receive the request
  // so that we can test the request end to end.
  client_->GetContent(focused_web_content_state().page_id, std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeMahiWebContentsManager::SetMahiBrowserDelegateForTesting(
    crosapi::mojom::MahiBrowserDelegate* delegate) {
  client_->SetMahiBrowserDelegateForTesting(delegate);
}
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
void FakeMahiWebContentsManager::BindMahiBrowserDelegateForTesting(
    mojo::PendingRemote<crosapi::mojom::MahiBrowserDelegate> pending_remote) {
  client_->BindMahiBrowserDelegateForTesting(std::move(pending_remote));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace mahi
