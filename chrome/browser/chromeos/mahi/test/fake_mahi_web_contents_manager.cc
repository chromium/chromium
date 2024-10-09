// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"

#include "base/notimplemented.h"
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

void FakeMahiWebContentsManager::RequestContent(
    const base::UnguessableToken& page_id,
    chromeos::mahi::GetContentCallback callback) {
  std::move(callback).Run(crosapi::mojom::MahiPageContent::New(
      /*client_id deprecated*/ base::UnguessableToken::Create(), page_id,
      u"Test page content"));
  ++number_of_request_content_calls_;
}

int FakeMahiWebContentsManager::GetNumberOfRequestContentCalls() {
  return number_of_request_content_calls_;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeMahiWebContentsManager::SetMahiBrowserDelegateForTesting(
    crosapi::mojom::MahiBrowserDelegate* delegate) {
  NOTIMPLEMENTED();
}
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
void FakeMahiWebContentsManager::BindMahiBrowserDelegateForTesting(
    mojo::PendingRemote<crosapi::mojom::MahiBrowserDelegate> pending_remote) {
  NOTIMPLEMENTED();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace mahi
