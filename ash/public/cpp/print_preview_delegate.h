// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PRINT_PREVIEW_DELEGATE_H_
#define ASH_PUBLIC_CPP_PRINT_PREVIEW_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom-forward.h"

namespace ash {

// A delegate to be implemented by chrome, used as a way to send ChromeOS
// print preview-related commands from ash webui to chrome.
class ASH_PUBLIC_EXPORT PrintPreviewDelegate {
 public:
  virtual ~PrintPreviewDelegate() = default;

  // Kick off the process to generate a print preview.
  // `token` is the unique identifier for the initiating webcontent.
  // `settings` is a struct of available print settings, refer to
  // printing/print_job_constants.h for relevant fields.
  // `callback` returns true if generating a preview has successfully queued.
  // False indicates that there was an issue with attempting to generate a
  // preview.
  virtual void StartGetPreview(const base::UnguessableToken& token,
                               crosapi::mojom::PrintSettingsPtr settings,
                               base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PRINT_PREVIEW_DELEGATE_H_
