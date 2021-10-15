// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SCREEN_CAPTURE_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SCREEN_CAPTURE_INFOBAR_DELEGATE_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/media_access_handler.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"

namespace content {
class WebContents;
}

// An infobar that allows the user to share their screen with the current page.
class ScreenCaptureInfoBarDelegateAndroid : public ConfirmInfoBarDelegate {
 public:
  // Creates a screen capture infobar and delegate and adds the infobar to the
  // infobars::ContentInfoBarManager associated with |web_contents|.
  static void Create(content::WebContents* web_contents,
                     const content::MediaStreamRequest& request,
                     content::MediaResponseCallback callback);

  ScreenCaptureInfoBarDelegateAndroid(
      const ScreenCaptureInfoBarDelegateAndroid&) = delete;
  ScreenCaptureInfoBarDelegateAndroid& operator=(
      const ScreenCaptureInfoBarDelegateAndroid&) = delete;

 private:
  ScreenCaptureInfoBarDelegateAndroid(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback);
  ~ScreenCaptureInfoBarDelegateAndroid() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetMessageText() const override;
  int GetIconId() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  void InfoBarDismissed() override;

  // Runs |callback_|, passing it the |result|, and (if permission was granted)
  // the appropriate stream device and UI object for video capture.
  void RunCallback(blink::mojom::MediaStreamRequestResult result);

  raw_ptr<content::WebContents> web_contents_;
  const content::MediaStreamRequest request_;
  content::MediaResponseCallback callback_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SCREEN_CAPTURE_INFOBAR_DELEGATE_ANDROID_H_
