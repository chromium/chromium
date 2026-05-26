// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TOAST_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TOAST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/messages/android/message_enums.h"

namespace content {
class WebContents;
}  // namespace content

namespace messages {
class MessageWrapper;
}  // namespace messages

namespace glic {

// A generalized helper class to show a Glic-branded message toast/banner on
// Android. The toast always displays Glic's spark icon and a standard "OK"
// primary button.
// TODO(crbug.com/515493573): Expand this to also support a Desktop variant
// (utilizing ToastController) to provide a fully platform-agnostic C++ toast
// interface for callers.
class GlicToast {
 public:
  // Creates and shows a Glic-branded toast message on the given `web_contents`.
  static std::unique_ptr<GlicToast> Show(content::WebContents* web_contents,
                                         int title_res_id,
                                         int description_res_id);

  GlicToast(content::WebContents* web_contents,
            int title_res_id,
            int description_res_id);
  ~GlicToast();

  GlicToast(const GlicToast&) = delete;
  GlicToast& operator=(const GlicToast&) = delete;

 private:
  void HandleMessageAccepted();
  void HandleMessageDismissed(messages::DismissReason reason);

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<messages::MessageWrapper> message_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_TOAST_H_
