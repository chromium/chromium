// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MALL_MALL_UI_DELEGATE_H_
#define ASH_WEBUI_MALL_MALL_UI_DELEGATE_H_

#include <string_view>

#include "base/functional/callback_forward.h"

class GURL;

namespace ash {

// Delegate which allows //chrome services to be exposed to the //ash WebUI.
class MallUIDelegate {
 public:
  virtual ~MallUIDelegate() = default;

  // Calls `callback` with a URL which can be used to embed the Mall website
  // into the WebUI.
  virtual void GetMallEmbedUrl(
      std::string_view path,
      base::OnceCallback<void(const GURL&)> callback) = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_MALL_MALL_UI_DELEGATE_H_
