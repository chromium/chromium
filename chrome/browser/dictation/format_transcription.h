// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_FORMAT_TRANSCRIPTION_H_
#define CHROME_BROWSER_DICTATION_FORMAT_TRANSCRIPTION_H_

#include <optional>
#include <string>
#include <string_view>

namespace dictation {

// Returns true if a whitespace separator is needed between `preceding_text`
// and `new_text`.
bool WhitespaceNeeded(std::u16string_view preceding_text,
                      std::u16string_view new_text);

// Formats transcription `text` for insertion into a target element given the
// text preceding the insertion point.
std::u16string FormatTranscription(
    const std::u16string& text,
    std::optional<std::u16string_view> preceding_text);

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_FORMAT_TRANSCRIPTION_H_
