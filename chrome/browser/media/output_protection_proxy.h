// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_PROXY_H_
#define CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_PROXY_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/display/output_protection_delegate.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/content_protection.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// A class to query output protection status and/or enable output protection.
//
// On Chrome OS, operations on the physical displays are delegated to
// OutputProtectionDelegate. On other platforms, physical displays are not
// checked.
//
// On all platforms, in ProcessQueryStatusResult(), this class checks the
// network link and adds it to the existing link mask.
//
// All methods except constructor should be invoked in UI thread.
class OutputProtectionProxy {
 public:
  using QueryStatusCallback =
      base::OnceCallback<void(bool /* success */,
                              uint32_t /* link_mask */,
                              uint32_t /* protection_mask*/)>;
  using EnableProtectionCallback = base::OnceCallback<void(bool /* success */)>;

  OutputProtectionProxy(int render_process_id, int render_frame_id);
  ~OutputProtectionProxy();

  void QueryStatus(QueryStatusCallback callback);
  void EnableProtection(uint32_t desired_method_mask,
                        EnableProtectionCallback callback);

 private:
  // Callbacks for QueryStatus(). It also checks the network link and adds it
  // to the |link_mask|.
  void ProcessQueryStatusResult(QueryStatusCallback callback,
                                bool success,
                                uint32_t link_mask,
                                uint32_t protection_mask);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ProcessQueryStatusResultLacros(
      QueryStatusCallback callback,
      crosapi::mojom::ContentProtectionWindowStatusPtr window_status);
#endif

  // Used to lookup the WebContents associated with the render frame.
  int render_process_id_;
  int render_frame_id_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::OutputProtectionDelegate output_protection_delegate_;
#endif

  base::WeakPtrFactory<OutputProtectionProxy> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_OUTPUT_PROTECTION_PROXY_H_
