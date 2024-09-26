// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_text.h"

namespace ash {

ScannerText::Line::Line() = default;
ScannerText::Line::Line(ScannerText::Line&& other) = default;
ScannerText::Line& ScannerText::Line::operator=(ScannerText::Line&& other) =
    default;
ScannerText::Line::~Line() = default;

ScannerText::Paragraph::Paragraph() = default;
ScannerText::Paragraph::Paragraph(ScannerText::Paragraph&& other) = default;
ScannerText::Paragraph& ScannerText::Paragraph::operator=(
    ScannerText::Paragraph&& other) = default;
ScannerText::Paragraph::~Paragraph() = default;

ScannerText::ScannerText() = default;
ScannerText::ScannerText(ScannerText&& other) = default;
ScannerText& ScannerText::operator=(ScannerText&& other) = default;
ScannerText::~ScannerText() = default;

}  // namespace ash
