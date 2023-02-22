// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/typography.h"

#include "ash/constants/ash_features.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

enum class FontFamily { kGoogleSans, kRoboto };

struct FontInfo {
  FontFamily family;
  gfx::Font::FontStyle style;
  int size;
  gfx::Font::Weight weight;
  int line_height;
};

std::vector<std::string> FontNames(FontFamily family) {
  switch (family) {
    case FontFamily::kGoogleSans:
      return {"Google Sans", "Roboto", "Noto Sans"};
    case FontFamily::kRoboto:
      return {"Roboto", "Noto Sans"};
  }
}

base::fixed_flat_map<TypographyToken, FontInfo, 21> MapFonts() {
  return base::MakeFixedFlatMap<TypographyToken, FontInfo>({
      /* Google Sans */
      {TypographyToken::kCrosDisplay0,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 57,
        gfx::Font::Weight::MEDIUM, 64}},
      {TypographyToken::kCrosDisplay1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 44,
        gfx::Font::Weight::MEDIUM, 52}},
      {TypographyToken::kCrosDisplay2,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 36,
        gfx::Font::Weight::MEDIUM, 44}},
      {TypographyToken::kCrosDisplay3,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 32,
        gfx::Font::Weight::MEDIUM, 40}},
      {TypographyToken::kCrosDisplay3Regular,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 32,
        gfx::Font::Weight::NORMAL, 40}},
      {TypographyToken::kCrosDisplay4,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 28,
        gfx::Font::Weight::MEDIUM, 36}},
      {TypographyToken::kCrosDisplay5,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 24,
        gfx::Font::Weight::MEDIUM, 32}},
      {TypographyToken::kCrosDisplay6,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 22,
        gfx::Font::Weight::MEDIUM, 28}},
      {TypographyToken::kCrosDisplay6Regular,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 22,
        gfx::Font::Weight::NORMAL, 28}},
      {TypographyToken::kCrosDisplay7,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 18,
        gfx::Font::Weight::MEDIUM, 24}},

      /* TODO(): Fix to render in Google Sans Text */
      {TypographyToken::kCrosTitle1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 16,
        gfx::Font::Weight::MEDIUM, 24}},
      {TypographyToken::kCrosHeadline1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 15,
        gfx::Font::Weight::MEDIUM, 22}},

      {TypographyToken::kCrosButton1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 14,
        gfx::Font::Weight::MEDIUM, 20}},
      {TypographyToken::kCrosButton2,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 13,
        gfx::Font::Weight::MEDIUM, 20}},

      {TypographyToken::kCrosBody0,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 16,
        gfx::Font::Weight::NORMAL, 24}},
      {TypographyToken::kCrosBody1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 14,
        gfx::Font::Weight::NORMAL, 20}},
      {TypographyToken::kCrosBody2,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 13,
        gfx::Font::Weight::NORMAL, 20}},

      {TypographyToken::kCrosAnnotation1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 12,
        gfx::Font::Weight::NORMAL, 18}},
      {TypographyToken::kCrosAnnotation2,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 11,
        gfx::Font::Weight::NORMAL, 16}},

      {TypographyToken::kCrosLabel1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 10,
        gfx::Font::Weight::MEDIUM, 10}},
      {TypographyToken::kCrosLabel2,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 10,
        gfx::Font::Weight::NORMAL, 10}},
  });
}

class TypographyProviderImpl : public TypographyProvider {
 public:
  TypographyProviderImpl() : font_map_(MapFonts()) {}

  ~TypographyProviderImpl() override = default;

  gfx::FontList ResolveTypographyToken(TypographyToken token) const override {
    const auto* iter = font_map_.find(token);
    if (iter == font_map_.end()) {
      NOTREACHED() << "Tried to resolve unmapped token";
      return gfx::FontList();
    }

    const FontInfo& info = iter->second;
    return gfx::FontList(FontNames(info.family), info.style, info.size,
                         info.weight);
  }

  int ResolveLineHeight(TypographyToken token) const override {
    const auto* iter = font_map_.find(token);
    if (iter == font_map_.end()) {
      NOTREACHED() << "Tried to resolve unmapped token";
      return 0;
    }

    return iter->second.line_height;
  }

 private:
  const base::fixed_flat_map<TypographyToken, FontInfo, 21> font_map_;
};

}  // namespace

// static
const TypographyProvider* TypographyProvider::Get() {
  static base::NoDestructor<TypographyProviderImpl> typography_provider;
  return typography_provider.get();
}

TypographyProvider::~TypographyProvider() = default;

void TypographyProvider::StyleLabel(TypographyToken token,
                                    views::Label& label) const {
  label.SetFontList(ResolveTypographyToken(token));
  label.SetLineHeight(ResolveLineHeight(token));
}

}  // namespace ash
