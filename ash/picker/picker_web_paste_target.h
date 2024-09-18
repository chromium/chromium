// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_PICKER_WEB_PASTE_TARGET_H_
#define ASH_PICKER_PICKER_WEB_PASTE_TARGET_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "url/gurl.h"

namespace ash {

// Wrapper around a `content::WebContents` for pasting directly, as well as
// containing information about the `content::WebContents` used for determining
// what contents to paste.
struct ASH_EXPORT PickerWebPasteTarget {
  explicit PickerWebPasteTarget(GURL url, base::OnceClosure do_paste);
  PickerWebPasteTarget(const PickerWebPasteTarget&) = delete;
  PickerWebPasteTarget& operator=(const PickerWebPasteTarget&) = delete;
  PickerWebPasteTarget(PickerWebPasteTarget&&);
  PickerWebPasteTarget& operator=(PickerWebPasteTarget&&);
  ~PickerWebPasteTarget();

  // The last committed URL of the `content::WebContents`.
  GURL url;
  // Call this to paste the current clipboard contents into the
  // `content::WebContents` associated with this struct.
  base::OnceClosure do_paste;
};

}  // namespace ash

#endif  // ASH_PICKER_PICKER_WEB_PASTE_TARGET_H_
