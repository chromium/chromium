// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"

#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

ChromePdfStreamDelegate::ChromePdfStreamDelegate() = default;
ChromePdfStreamDelegate::~ChromePdfStreamDelegate() = default;

absl::optional<pdf::PdfStreamDelegate::StreamInfo>
ChromePdfStreamDelegate::GetStreamInfo(content::WebContents* contents) {
  extensions::MimeHandlerViewGuest* guest =
      extensions::MimeHandlerViewGuest::FromWebContents(contents);
  if (!guest)
    return absl::nullopt;

  base::WeakPtr<extensions::StreamContainer> stream = guest->GetStreamWeakPtr();
  if (!stream)
    return absl::nullopt;

  if (stream->extension_id() != extension_misc::kPdfExtensionId)
    return absl::nullopt;

  if (!stream->pdf_plugin_attributes())
    return absl::nullopt;

  return StreamInfo{
      .stream_url = stream->stream_url(),
      .original_url = stream->original_url(),
      .background_color = base::checked_cast<SkColor>(
          stream->pdf_plugin_attributes()->background_color),
      .full_frame = !stream->embedded(),
      .allow_javascript = stream->pdf_plugin_attributes()->allow_javascript,
  };
}
