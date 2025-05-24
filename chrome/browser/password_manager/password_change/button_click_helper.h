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

// A helper class which performs a click on specified element based on
// `dom_node_id`. Invokes `callback` on completing.
class ButtonClickHelper {
 public:
  using ClickResult = base::OnceCallback<void(bool)>;

  ButtonClickHelper(content::WebContents* web_contents,
                    int dom_node_id,
                    ClickResult callback);
  ~ButtonClickHelper();

#if defined(UNIT_TEST)
  void SimulateClickResult(bool result) { std::move(callback_).Run(result); }
#endif

 private:
  void OnButtonClicked(actor::mojom::ActionResultPtr result);

  ClickResult callback_;

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;

  base::WeakPtrFactory<ButtonClickHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_BUTTON_CLICK_HELPER_H_
