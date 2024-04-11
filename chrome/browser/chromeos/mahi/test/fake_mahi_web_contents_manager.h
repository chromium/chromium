// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_TEST_FAKE_MAHI_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_TEST_FAKE_MAHI_WEB_CONTENTS_MANAGER_H_

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"

#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/image/image_skia.h"

namespace mahi {

// Fake class for testing `MahiWebContentsManager`. It allows overriding the
// mojom connections to the utility process and ash chrome. It also provide
// access to the local variables, such as the web content states, so that we can
// easily check them in the test.
class FakeMahiWebContentsManager : public MahiWebContentsManager {
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

  WebContentState requested_web_content_state() {
    return requested_web_content_state_;
  }

  void RequestContentFromPage(const base::UnguessableToken& page_id,
                              GetContentCallback callback);

  bool GetPrefValue() const override;
  void SetPrefForTesting(bool pref_state) { pref_state_ = pref_state; }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetMahiBrowserDelegateForTesting(
      crosapi::mojom::MahiBrowserDelegate* delegate);
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
  void BindMahiBrowserDelegateForTesting(
      mojo::PendingRemote<crosapi::mojom::MahiBrowserDelegate> pending_remote);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  bool pref_state_ = true;
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_TEST_FAKE_MAHI_WEB_CONTENTS_MANAGER_H_
