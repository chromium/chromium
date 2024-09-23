// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_TYPOGRAPHY_H_
#define ASH_STYLE_TYPOGRAPHY_H_

#include "ash/ash_export.h"
#include "ui/gfx/font_list.h"

namespace views {
class Label;
}

namespace ash {

enum class ASH_EXPORT TypographyToken {
  /* Legacy tokens */
  kLegacyDisplay1,
  kLegacyDisplay2,
  kLegacyDisplay3,
  kLegacyDisplay4,
  kLegacyDisplay5,
  kLegacyDisplay6,
  kLegacyDisplay7,
  kLegacyTitle1,
  kLegacyTitle2,
  kLegacyHeadline1,
  kLegacyHeadline2,
  kLegacyButton1,
  kLegacyButton2,
  kLegacyBody1,
  kLegacyBody2,
  kLegacyAnnotation1,
  kLegacyAnnotation2,
  kLegacyLabel1,
  kLegacyLabel2,
  kLastLegacyToken = kLegacyLabel2,

  // cros.typography tokens
  kCrosDisplay0,
  kCrosDisplay1,
  kCrosDisplay2,
  kCrosDisplay3,
  kCrosDisplay3Regular,
  kCrosDisplay4,
  kCrosDisplay5,
  kCrosDisplay6,
  kCrosDisplay6Regular,
  kCrosDisplay7,

  kCrosTitle1,
  kCrosTitle2,
  kCrosHeadline1,

  kCrosButton1,
  kCrosButton2,

  kCrosBody0,
  kCrosBody1,
  kCrosBody2,

  kCrosAnnotation1,
  kCrosAnnotation2,

  kCrosLabel1,
  kCrosLabel2,
  kMaxValue = kCrosLabel2,
};

class ASH_EXPORT TypographyProvider {
 public:
  // Returns the global `TypographyProvider` for Ash.
  static const TypographyProvider* Get();

  TypographyProvider() = default;
  virtual ~TypographyProvider();

  // Applies the appropriate style for `token` to `label`.
  virtual void StyleLabel(TypographyToken token, views::Label& label) const;

  // For the given `token` returns the appropriately configured `FontList`. For
  // legacy tokens, returns the old or new styling depending on the Jelly flag.
  virtual gfx::FontList ResolveTypographyToken(TypographyToken token) const = 0;

  // Returns the line height in pixels for `token`.
  virtual int ResolveLineHeight(TypographyToken token) const = 0;
};

}  // namespace ash

#endif  // ASH_STYLE_TYPOGRAPHY_H_
