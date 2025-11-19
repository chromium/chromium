// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_TYPING_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_TYPING_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class WebContents;
}

// A simple wrapper around actor::mojom::TypeAction.
class TypingHelper {
 public:
  using TypingResult = base::OnceCallback<void(actor::mojom::ActionResultCode)>;

  TypingHelper(content::WebContents* web_contents,
               int dom_node_id,
               const std::u16string& value,
               TypingResult callback);
  ~TypingHelper();

#if defined(UNIT_TEST)
  void SimulateTypingResult(bool result) {
    std::move(callback_).Run(
        result ? actor::mojom::ActionResultCode::kOk
               : actor::mojom::ActionResultCode::kInvalidDomNodeId);
  }
  int dom_node_id() const { return dom_node_id_; }
#endif

 private:
  void OnTyped(actor::mojom::ActionResultPtr result);

  const int dom_node_id_;
  TypingResult callback_;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;
  base::WeakPtrFactory<TypingHelper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_TYPING_HELPER_H_
