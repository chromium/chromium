// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_TEXT_H_
#define ASH_SCANNER_SCANNER_TEXT_H_

#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/span.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace ash {

// Contains text data retrieved during a Scanner session. Includes metadata
// needed to present the text, e.g. in an overlay to indicate detected text,
// translations, etc.
class ASH_EXPORT ScannerText {
 public:
  // A bounding box rotated around its center.
  struct ASH_EXPORT CenterRotatedBox {
    // Center of the box in coordinates of the containing overlay.
    gfx::Point center;

    // Size of the box in coordinates of the containing overlay.
    gfx::Size size;

    // Clockwise rotation around the center in degrees.
    float rotation = 0;

    bool operator==(const CenterRotatedBox&) const;
  };

  // A line of text.
  class ASH_EXPORT Line {
   public:
    Line(const gfx::Range& range, const CenterRotatedBox& bounding_box);
    Line(Line&& other);
    Line& operator=(Line&& other);
    ~Line();

    const gfx::Range& range() const { return range_; }

    const CenterRotatedBox& bounding_box() const { return bounding_box_; }

   private:
    // The UTF-16 range of the substring of `text_contents_` formed by this
    // line.
    gfx::Range range_;

    // The bounding box containing the line, in coordinates of the containing
    // overlay.
    CenterRotatedBox bounding_box_;
  };

  // A paragraph of text.
  class ASH_EXPORT Paragraph {
   public:
    Paragraph();
    Paragraph(Paragraph&& other);
    Paragraph& operator=(Paragraph&& other);
    ~Paragraph();

    base::span<const Line> lines() const { return lines_; }

    // Appends a new line to the paragraph. `range` specifies the UTF-16 range
    // of `text_contents_` corresponding to the line and `bounding_box`
    // specifies the bounds of the line in coordinates of the containing
    // overlay. Note that previous Line references may be invalidated after
    // calling this method (since `lines_` is resized).
    void AppendLine(const gfx::Range& range,
                    const CenterRotatedBox& bounding_box);

   private:
    // List of lines in natural reading order.
    std::vector<Line> lines_;
  };

  explicit ScannerText(std::u16string_view text_contents);
  ScannerText(ScannerText&& other);
  ScannerText& operator=(ScannerText&& other);
  ~ScannerText();

  base::span<const Paragraph> paragraphs() const { return paragraphs_; }

  // Appends a new paragraph and returns a reference to the created paragraph.
  // Note that previous Paragraph references may be invalidated after calling
  // this method (since `paragraphs_` is resized).
  Paragraph& AppendParagraph();

  // Gets the substring of `text_contents_` for the UTF-16 indices specified by
  // `range`. Returns an empty string if the range is invalid.
  std::u16string GetTextFromRange(const gfx::Range& range) const;

 private:
  // List of paragraphs in natural reading order.
  std::vector<Paragraph> paragraphs_;

  // The full text contents.
  std::u16string text_contents_;
};

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_TEXT_H_
