// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_BUTTON_CLICK_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_BUTTON_CLICK_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;
}

namespace password_manager {
class PasswordManagerClient;
}

// A helper class which performs a click on specified element based on
// `dom_node_id`. Invokes `callback` on completing.
class ButtonClickHelper {
 public:
  using ClickResult = base::OnceCallback<void(actor::mojom::ActionResultCode)>;

  ButtonClickHelper(content::WebContents* web_contents,
                    password_manager::PasswordManagerClient* client,
                    int dom_node_id,
                    ClickResult callback);
  ~ButtonClickHelper();

#if defined(UNIT_TEST)
  void SimulateClickResult(bool result) {
    std::move(callback_).Run(
        result ? actor::mojom::ActionResultCode::kOk
               : actor::mojom::ActionResultCode::kInvalidDomNodeId);
  }
#endif

 private:
  void OnButtonClicked(actor::mojom::ActionResultPtr result);

  ClickResult callback_;

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;
  raw_ptr<password_manager::PasswordManagerClient> client_ = nullptr;

  base::WeakPtrFactory<ButtonClickHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_BUTTON_CLICK_HELPER_H_
