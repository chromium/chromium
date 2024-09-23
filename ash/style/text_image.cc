// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/text_image.h"

#include "ash/style/typography.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"

namespace ash {

namespace {

gfx::ImageSkia Generator(const gfx::Size& size,
                         base_icu::UChar32 symbol,
                         ui::ColorId color_id,
                         const ui::ColorProvider* color_provider) {
  auto text_image = std::make_unique<TextImage>(size, symbol);
  text_image->set_color(color_provider->GetColor(color_id));
  return gfx::ImageSkia(std::move(text_image), size);
}

std::u16string ToString(base_icu::UChar32 symbol) {
  std::u16string str;
  base::WriteUnicodeCharacter(symbol, &str);
  return str;
}

}  // namespace

TextImage::TextImage(const gfx::Size& size, base_icu::UChar32 symbol)
    : gfx::CanvasImageSource(size), symbol_(ToString(symbol)) {}

TextImage::~TextImage() = default;

// static
ui::ImageModel TextImage::AsImageModel(const gfx::Size& size,
                                       base_icu::UChar32 symbol,
                                       ui::ColorId color_id) {
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(&Generator, size, symbol, color_id), size);
}

void TextImage::Draw(gfx::Canvas* canvas) {
  const int font_size = size().height();
  const gfx::FontList font({"Google Sans", "Roboto"}, gfx::Font::NORMAL,
                           font_size, gfx::Font::Weight::MEDIUM);
  canvas->DrawStringRectWithFlags(symbol_, font, color_, gfx::Rect(size()),
                                  gfx::Canvas::TEXT_ALIGN_CENTER);
}

}  // namespace ash
