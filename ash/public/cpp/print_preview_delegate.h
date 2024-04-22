// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PRINT_PREVIEW_DELEGATE_H_
#define ASH_PUBLIC_CPP_PRINT_PREVIEW_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/unguessable_token.h"

namespace ash {

// A delegate to be implemented by chrome, used as a way to send ChromeOS
// print preview-related commands from ash webui to chrome.
class ASH_PUBLIC_EXPORT PrintPreviewDelegate {
 public:
  virtual ~PrintPreviewDelegate() = default;

  // Kick off the process to generate a print preview.
  virtual void StartGetPreview(base::UnguessableToken token) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRINT_PREVIEW_DELEGATE_H_
