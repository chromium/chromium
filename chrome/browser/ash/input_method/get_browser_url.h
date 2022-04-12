// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_GET_BROWSER_URL_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_GET_BROWSER_URL_H_

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {

using GetFocusedTabUrlCallback =
    base::OnceCallback<void(const absl::optional<GURL>&)>;

void GetFocusedTabUrl(GetFocusedTabUrlCallback callback);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_GET_BROWSER_URL_H_
