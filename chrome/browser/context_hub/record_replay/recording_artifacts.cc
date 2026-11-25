// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

bool ShouldStripRecordedCodePoint(base_icu::UChar32 code_point) {
  // Strip C0/C1 control characters except Unicode whitespace, which is
  // collapsed to ASCII spaces.
  if (base::IsUnicodeControl(code_point) &&
      !base::IsUnicodeWhitespace(code_point)) {
    return true;
  }
  // Strip soft hyphen, directional marks, zero-width / default-ignorable format
  // characters, bidi overrides/isolates, and variation selectors.
  if (code_point == 0x00AD || code_point == 0x061C || code_point == 0x180E ||
      (code_point >= 0x200B && code_point <= 0x200F) ||
      (code_point >= 0x202A && code_point <= 0x202E) ||
      (code_point >= 0x2060 && code_point <= 0x206F) ||
      (code_point >= 0xFE00 && code_point <= 0xFE0F) || code_point == 0xFEFF ||
      (code_point >= 0xFFF9 && code_point <= 0xFFFB)) {
    return true;
  }
  // Strip Unicode Tag (`U+E0000..U+E007F`) and Variation Selector Supplement
  // (`U+E0100..U+E01EF`) characters.
  return (code_point >= 0xE0000 && code_point <= 0xE007F) ||
         (code_point >= 0xE0100 && code_point <= 0xE01EF);
}

}  // namespace

GURL SanitizeRecordedUrl(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return GURL();
  }
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearRef();
  GURL sanitized = url.ReplaceComponents(replacements);
  if (sanitized.spec().size() > kMaxRecordedUrlBytes) {
    GURL::Replacements clear_path;
    clear_path.ClearPath();
    sanitized = sanitized.ReplaceComponents(clear_path);
    if (sanitized.spec().size() > kMaxRecordedUrlBytes) {
      return GURL();
    }
  }
  return sanitized;
}

std::string SanitizeRecordedText(std::string_view text, size_t max_bytes) {
  if (max_bytes == 0) {
    return {};
  }

  std::string out;
  out.reserve(std::min(text.size(), max_bytes));
  bool pending_space = false;
  bool truncated = false;

  for (size_t i = 0; i < text.size(); ++i) {
    base_icu::UChar32 code_point = 0;
    if (!base::ReadUnicodeCharacter(text, &i, &code_point) ||
        !base::IsValidCharacter(code_point)) {
      code_point = 0xFFFD;
    }
    if (ShouldStripRecordedCodePoint(code_point)) {
      continue;
    }
    if (base::IsUnicodeWhitespace(code_point)) {
      if (!out.empty()) {
        pending_space = true;
      }
      continue;
    }

    if (pending_space) {
      if (out.size() + 1 >= max_bytes) {
        truncated = true;
        break;
      }
      out.push_back(' ');
      pending_space = false;
    }
    base::WriteUnicodeCharacter(code_point, &out);
    if (out.size() > max_bytes) {
      truncated = true;
      break;
    }
  }

  if (!truncated) {
    return out;
  }
  constexpr std::string_view kEllipsis = "...";
  if (max_bytes <= kEllipsis.size()) {
    return std::string(base::TrimWhitespaceASCII(
        base::TruncateUTF8ToByteSize(out, max_bytes), base::TRIM_TRAILING));
  }
  return base::StrCat(
      {base::TrimWhitespaceASCII(
           base::TruncateUTF8ToByteSize(out, max_bytes - kEllipsis.size()),
           base::TRIM_TRAILING),
       kEllipsis});
}

RecordedAction::RecordedAction(
    browser::context_hub::mojom::SkillRecordingActionType type,
    std::string description,
    base::TimeDelta timestamp)
    : type(type), description(std::move(description)), timestamp(timestamp) {}

RecordedAction::RecordedAction(RecordedAction&&) noexcept = default;
RecordedAction& RecordedAction::operator=(RecordedAction&&) noexcept = default;
RecordedAction::~RecordedAction() = default;

RecordedTranscript::RecordedTranscript(std::string text,
                                       base::TimeDelta timestamp)
    : text(std::move(text)), timestamp(timestamp) {}

RecordedTranscript::RecordedTranscript(RecordedTranscript&&) noexcept = default;
RecordedTranscript& RecordedTranscript::operator=(
    RecordedTranscript&&) noexcept = default;
RecordedTranscript::~RecordedTranscript() = default;

RecordingArtifacts::RecordingArtifacts() = default;
RecordingArtifacts::RecordingArtifacts(RecordingArtifacts&&) noexcept = default;
RecordingArtifacts& RecordingArtifacts::operator=(
    RecordingArtifacts&&) noexcept = default;
RecordingArtifacts::~RecordingArtifacts() = default;

}  // namespace context_hub
