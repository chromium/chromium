// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_TEXT_H_
#define ASH_SCANNER_SCANNER_TEXT_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace ash {

// Contains text data retrieved during a Scanner session. Includes metadata
// needed to present the text, e.g. in an overlay to indicate detected text,
// translations, etc.
struct ASH_EXPORT ScannerText {
  // A bounding box rotated around its center.
  struct CenterRotatedBox {
    // Center of the box in coordinates of the containing overlay.
    gfx::Point center;

    // Size of the box in coordinates of the containing overlay.
    gfx::Size size;

    // Clockwise rotation around the center in degrees.
    float rotation = 0;
  };

  // A line of text.
  struct Line {
    Line();
    Line(Line&& other);
    Line& operator=(Line&& other);
    ~Line();

    // The UTF-16 range of the substring of `text_contents` formed by this line.
    gfx::Range range;

    // The bounding box containing the line.
    CenterRotatedBox bounding_box;
  };

  // A paragraph of text.
  struct Paragraph {
    Paragraph();
    Paragraph(Paragraph&& other);
    Paragraph& operator=(Paragraph&& other);
    ~Paragraph();

    // List of lines in natural reading order.
    std::vector<Line> lines;
  };

  ScannerText();
  ScannerText(ScannerText&& other);
  ScannerText& operator=(ScannerText&& other);
  ~ScannerText();

  // List of paragraphs in natural reading order.
  std::vector<Paragraph> paragraphs;

  // The full text contents.
  std::u16string text_contents;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_TEXT_H_
