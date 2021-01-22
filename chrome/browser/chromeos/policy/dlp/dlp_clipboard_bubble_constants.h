// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_BUBBLE_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_BUBBLE_CONSTANTS_H_

namespace policy {

// Clipboard ARC toast ID.
constexpr char kClipboardDlpArcToastId[] = "clipboard_dlp_block_arc";

// Clipboard Crostini toast ID.
constexpr char kClipboardDlpCrostiniToastId[] = "clipboard_dlp_block_crostini";

// Clipboard Plugin VM toast ID.
constexpr char kClipboardDlpPluginVmToastId[] = "clipboard_dlp_block_plugin_vm";

// The duration of the clipboard toast/bubble shown on blocked paste.
constexpr int kClipboardDlpBlockDurationMs = 2500;

// The duration of the clipboard warning shown before paste.
constexpr int kClipboardDlpWarnDurationMs = 20000;

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_BUBBLE_CONSTANTS_H_
