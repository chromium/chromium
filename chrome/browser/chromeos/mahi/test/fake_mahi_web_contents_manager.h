// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_TEST_FAKE_MAHI_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_TEST_FAKE_MAHI_WEB_CONTENTS_MANAGER_H_

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager_impl.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/image/image_skia.h"

namespace mahi {

// Fake class for testing `MahiWebContentsManager`. It allows overriding the
// mojom connections to the utility process and ash chrome. It also provide
// access to the local variables, such as the web content states, so that we can
// easily check them in the test.
class FakeMahiWebContentsManager : public MahiWebContentsManagerImpl {
 public:
  FakeMahiWebContentsManager();

  FakeMahiWebContentsManager(const FakeMahiWebContentsManager&) = delete;
  FakeMahiWebContentsManager& operator=(const FakeMahiWebContentsManager&) =
      delete;

  ~FakeMahiWebContentsManager() override;

  gfx::ImageSkia GetFavicon(content::WebContents* web_contents) const override;

  WebContentState focused_web_content_state() {
    return focused_web_content_state_;
  }

  void set_focused_web_content_is_distillable(bool value) {
    focused_web_content_state_.is_distillable.emplace(value);
  }

  void RequestContent(const base::UnguessableToken& page_id,
                      chromeos::mahi::GetContentCallback callback) override;

  int GetNumberOfRequestContentCalls();

  bool is_pdf_focused_web_contents() { return is_pdf_focused_web_contents_; }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetMahiBrowserDelegateForTesting(
      crosapi::mojom::MahiBrowserDelegate* delegate);
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
  void BindMahiBrowserDelegateForTesting(
      mojo::PendingRemote<crosapi::mojom::MahiBrowserDelegate> pending_remote);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  int number_of_request_content_calls_ = 0;
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_TEST_FAKE_MAHI_WEB_CONTENTS_MANAGER_H_
