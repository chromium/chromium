// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_text.h"

#include <string>
#include <string_view>

#include "ui/gfx/range/range.h"

namespace ash {

bool ScannerText::CenterRotatedBox::operator==(
    const ScannerText::CenterRotatedBox&) const = default;

ScannerText::Line::Line(const gfx::Range& range,
                        const CenterRotatedBox& bounding_box)
    : range_(range), bounding_box_(bounding_box) {}
ScannerText::Line::Line(ScannerText::Line&& other) = default;
ScannerText::Line& ScannerText::Line::operator=(ScannerText::Line&& other) =
    default;
ScannerText::Line::~Line() = default;

ScannerText::Paragraph::Paragraph() = default;
ScannerText::Paragraph::Paragraph(ScannerText::Paragraph&& other) = default;
ScannerText::Paragraph& ScannerText::Paragraph::operator=(
    ScannerText::Paragraph&& other) = default;
ScannerText::Paragraph::~Paragraph() = default;

void ScannerText::Paragraph::AppendLine(
    const gfx::Range& range,
    const ScannerText::CenterRotatedBox& bounding_box) {
  lines_.emplace_back(range, bounding_box);
}

ScannerText::ScannerText(std::u16string_view text_contents)
    : text_contents_(text_contents) {}
ScannerText::ScannerText(ScannerText&& other) = default;
ScannerText& ScannerText::operator=(ScannerText&& other) = default;
ScannerText::~ScannerText() = default;

ScannerText::Paragraph& ScannerText::AppendParagraph() {
  paragraphs_.emplace_back();
  return paragraphs_.back();
}

std::u16string ScannerText::GetTextFromRange(const gfx::Range& range) const {
  if (!range.IsValid() || range.GetMax() > text_contents_.length()) {
    return u"";
  }
  return text_contents_.substr(range.GetMin(), range.length());
}

}  // namespace ash
