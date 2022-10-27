// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_BUBBLE_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_BUBBLE_CONSTANTS_H_

namespace policy {

// Clipboard ARC toast ID in block mode.
constexpr char kClipboardBlockArcToastId[] = "clipboard_dlp_block_arc";

// Clipboard ARC toast ID in warning mode.
constexpr char kClipboardWarnArcToastId[] = "clipboard_dlp_warn_arc";

// Clipboard Crostini toast ID in block mode.
constexpr char kClipboardBlockCrostiniToastId[] =
    "clipboard_dlp_block_crostini";

// Clipboard Crostini toast ID in warning mode.
constexpr char kClipboardWarnCrostiniToastId[] = "clipboard_dlp_warn_crostini";

// Clipboard Plugin VM toast ID in block mode.
constexpr char kClipboardBlockPluginVmToastId[] =
    "clipboard_dlp_block_plugin_vm";

// Clipboard Plugin VM toast ID in warning mode.
constexpr char kClipboardWarnPluginVmToastId[] = "clipboard_dlp_warn_plugin_vm";

// The duration of the clipboard bubble shown on blocked paste.
constexpr int kClipboardDlpBlockDurationMs = 6000;

// The duration of the clipboard warning shown before paste.
constexpr int kClipboardDlpWarnDurationMs = 16000;

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CLIPBOARD_BUBBLE_CONSTANTS_H_
