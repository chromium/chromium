// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_
#define ASH_PUBLIC_CPP_PICKER_PICKER_CLIENT_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/ash_web_view.h"
#include "base/functional/callback_forward.h"

class GURL;

namespace ash {

// Lets PickerController in Ash to communicate with the browser.
class ASH_PUBLIC_EXPORT PickerClient {
 public:
  using DownloadGifToStringCallback =
      base::OnceCallback<void(const std::string& gif_data)>;

  virtual std::unique_ptr<ash::AshWebView> CreateWebView(
      const ash::AshWebView::InitParams& params) = 0;

  // Downloads a gif from `url`. If the download is successful, the gif is
  // passed to `callback` as a string of encoded bytes in gif format. Otherwise,
  // `callback` is run with an empty string.
  virtual void DownloadGifToString(const GURL& url,
                                   DownloadGifToStringCallback callback) = 0;

 protected:
  PickerClient();
  virtual ~PickerClient();
};

}  // namespace ash

#endif
