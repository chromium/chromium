// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/typography.h"

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "chromeos/constants/chromeos_features.h"
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
  // Fallback to the appropriate Noto Sans happens through the linux font
  // fallback logic.
  switch (family) {
    case FontFamily::kGoogleSans:
      return {"Google Sans", "Roboto"};
    case FontFamily::kRoboto:
      return {"Roboto"};
  }
}

// Maps legacy typography tokens to their cros.sys equivalents.
constexpr auto kTokenEquivalents =
    base::MakeFixedFlatMap<TypographyToken, TypographyToken>({
        {TypographyToken::kLegacyDisplay1, TypographyToken::kCrosDisplay1},
        {TypographyToken::kLegacyDisplay2, TypographyToken::kCrosDisplay2},
        {TypographyToken::kLegacyDisplay3, TypographyToken::kCrosDisplay3},
        {TypographyToken::kLegacyDisplay4, TypographyToken::kCrosDisplay4},
        {TypographyToken::kLegacyDisplay5, TypographyToken::kCrosDisplay5},
        {TypographyToken::kLegacyDisplay6, TypographyToken::kCrosDisplay6},
        {TypographyToken::kLegacyDisplay7, TypographyToken::kCrosDisplay7},

        {TypographyToken::kLegacyTitle1, TypographyToken::kCrosTitle1},
        // Since `kCrosTitle1` is Google Sans Text, its appropriate for both.
        {TypographyToken::kLegacyTitle2, TypographyToken::kCrosTitle1},

        {TypographyToken::kLegacyHeadline1, TypographyToken::kCrosHeadline1},
        // Since `kCrosHeadline1` is Google Sans Text, its appropriate for both.
        {TypographyToken::kLegacyHeadline2, TypographyToken::kCrosHeadline1},

        {TypographyToken::kLegacyButton1, TypographyToken::kCrosButton1},
        {TypographyToken::kLegacyButton2, TypographyToken::kCrosButton2},

        {TypographyToken::kLegacyBody1, TypographyToken::kCrosBody1},
        {TypographyToken::kLegacyBody2, TypographyToken::kCrosBody2},

        {TypographyToken::kLegacyAnnotation1,
         TypographyToken::kCrosAnnotation1},
        {TypographyToken::kLegacyAnnotation2,
         TypographyToken::kCrosAnnotation2},

        {TypographyToken::kLegacyLabel1, TypographyToken::kCrosLabel1},
        {TypographyToken::kLegacyLabel2, TypographyToken::kCrosLabel2},
    });

// Returns a map of tokens to `FontInfo`.
base::fixed_flat_map<TypographyToken, FontInfo, 40> MapFonts() {
  return base::MakeFixedFlatMap<TypographyToken, FontInfo>({
      /* Legacy tokens */
      {TypographyToken::kLegacyDisplay1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 44,
        gfx::Font::Weight::MEDIUM, 52}},
      {TypographyToken::kLegacyDisplay2,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 36,
        gfx::Font::Weight::MEDIUM, 44}},
      {TypographyToken::kLegacyDisplay3,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 32,
        gfx::Font::Weight::MEDIUM, 40}},
      {TypographyToken::kLegacyDisplay4,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 28,
        gfx::Font::Weight::MEDIUM, 36}},
      {TypographyToken::kLegacyDisplay5,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 24,
        gfx::Font::Weight::MEDIUM, 32}},
      {TypographyToken::kLegacyDisplay6,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 22,
        gfx::Font::Weight::MEDIUM, 28}},
      {TypographyToken::kLegacyDisplay7,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 18,
        gfx::Font::Weight::MEDIUM, 24}},

      {TypographyToken::kLegacyTitle1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 16,
        gfx::Font::Weight::MEDIUM, 24}},
      {TypographyToken::kLegacyTitle2,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 16, gfx::Font::Weight::MEDIUM,
        24}},

      {TypographyToken::kLegacyHeadline1,
       {FontFamily::kGoogleSans, gfx::Font::NORMAL, 15,
        gfx::Font::Weight::MEDIUM, 22}},
      {TypographyToken::kLegacyHeadline2,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 15, gfx::Font::Weight::MEDIUM,
        22}},

      {TypographyToken::kLegacyButton1,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 14, gfx::Font::Weight::MEDIUM,
        20}},
      {TypographyToken::kLegacyButton2,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 13, gfx::Font::Weight::MEDIUM,
        20}},

      {TypographyToken::kLegacyBody1,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 14, gfx::Font::Weight::NORMAL,
        20}},
      {TypographyToken::kLegacyBody2,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 13, gfx::Font::Weight::NORMAL,
        20}},

      {TypographyToken::kLegacyAnnotation1,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 12, gfx::Font::Weight::NORMAL,
        18}},
      {TypographyToken::kLegacyAnnotation2,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 11, gfx::Font::Weight::NORMAL,
        16}},

      {TypographyToken::kLegacyLabel1,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 10, gfx::Font::Weight::MEDIUM,
        10}},
      {TypographyToken::kLegacyLabel2,
       {FontFamily::kRoboto, gfx::Font::NORMAL, 10, gfx::Font::Weight::NORMAL,
        10}},

      /* cros.typography tokens */
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

      /* TODO(b/256663656): Fix to render in Google Sans Text */
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
    const FontInfo& info = LookupInfo(token);
    return gfx::FontList(FontNames(info.family), info.style, info.size,
                         info.weight);
  }

  int ResolveLineHeight(TypographyToken token) const override {
    return LookupInfo(token).line_height;
  }

 private:
  const FontInfo& LookupInfo(TypographyToken token) const {
    TypographyToken converted_token = ConvertToken(token);
    const auto* iter = font_map_.find(converted_token);
    if (iter == font_map_.end()) {
      NOTREACHED() << "Tried to resolve unmapped token";
      return font_map_.at(TypographyToken::kLegacyDisplay1);
    }
    return iter->second;
  }

  // Returns the equivalient cros.sys token for a legacy token if styles should
  // be converted.
  TypographyToken ConvertToken(TypographyToken token) const {
    if (!chromeos::features::IsJellyEnabled() ||
        token > TypographyToken::kLastLegacyToken) {
      return token;
    }

    const auto* iter = kTokenEquivalents.find(token);
    if (iter == kTokenEquivalents.end()) {
      NOTREACHED() << "Missing a mapping for legacy token "
                   << static_cast<int>(token);
      // Return an arbitrary but valid cros.sys token.
      return TypographyToken::kCrosButton1;
    }

    return iter->second;
  }

  const base::fixed_flat_map<TypographyToken, FontInfo, 40> font_map_;
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
