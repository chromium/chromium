// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/fontconfig_util_linux.h"

#include <fontconfig/fontconfig.h>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"

namespace base {

namespace {

const char kFontsConfTemplate[] = R"(<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>

  <!-- Cache location. -->
  <cachedir>$1</cachedir>

  <!-- GCS-synced fonts. -->
  <dir>$2</dir>

  <!-- Default properties. -->
  <match target="font">
    <edit name="embeddedbitmap" mode="append_last">
      <bool>false</bool>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Times</string>
    </test>
    <edit name="family" mode="assign">
      <string>Tinos</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>sans</string>
    </test>
    <edit name="family" mode="assign">
      <string>DejaVu Sans</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>sans serif</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
  </match>

  <!-- Some layout tests specify Helvetica as a family and we need to make sure
       that we don't fallback to Tinos for them -->
  <match target="pattern">
    <test qual="any" name="family">
      <string>Helvetica</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>sans-serif</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>serif</string>
    </test>
    <edit name="family" mode="assign">
      <string>Tinos</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>mono</string>
    </test>
    <edit name="family" mode="assign">
      <string>Cousine</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>monospace</string>
    </test>
    <edit name="family" mode="assign">
      <string>Cousine</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Courier</string>
    </test>
    <edit name="family" mode="assign">
      <string>Cousine</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>cursive</string>
    </test>
    <edit name="family" mode="assign">
      <string>Comic Sans MS</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>fantasy</string>
    </test>
    <edit name="family" mode="assign">
      <string>Impact</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Monaco</string>
    </test>
    <edit name="family" mode="assign">
      <string>Tinos</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Arial</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Courier New</string>
    </test>
    <edit name="family" mode="assign">
      <string>Cousine</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Georgia</string>
    </test>
    <edit name="family" mode="assign">
      <string>Gelasio</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Times New Roman</string>
    </test>
    <edit name="family" mode="assign">
      <string>Tinos</string>
    </edit>
  </match>

  <match target="pattern">
    <test qual="any" name="family">
      <string>Verdana</string>
    </test>
    <!-- NOT metrically compatible! -->
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
  </match>

  <!-- TODO(thomasanderson): Move these configs to be test-specific. -->
  <match target="pattern">
    <test name="family" compare="eq">
      <string>NonAntiAliasedSans</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
    <edit name="antialias" mode="assign">
      <bool>false</bool>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>SlightHintedGeorgia</string>
    </test>
    <edit name="family" mode="assign">
      <string>Gelasio</string>
    </edit>
    <edit name="hintstyle" mode="assign">
      <const>hintslight</const>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>NonHintedSans</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
    <!-- These deliberately contradict each other. The 'hinting' preference
         should take priority -->
    <edit name="hintstyle" mode="assign">
      <const>hintfull</const>
    </edit>
   <edit name="hinting" mode="assign">
      <bool>false</bool>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>AutohintedSerif</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
    <edit name="autohint" mode="assign">
      <bool>true</bool>
    </edit>
    <edit name="hintstyle" mode="assign">
      <const>hintmedium</const>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>HintedSerif</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
    <edit name="autohint" mode="assign">
      <bool>false</bool>
    </edit>
    <edit name="hintstyle" mode="assign">
      <const>hintmedium</const>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>FullAndAutoHintedSerif</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
    <edit name="autohint" mode="assign">
      <bool>true</bool>
    </edit>
    <edit name="hintstyle" mode="assign">
      <const>hintfull</const>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>SubpixelEnabledArial</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
    <edit name="rgba" mode="assign">
      <const>rgb</const>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>SubpixelDisabledArial</string>
    </test>
    <edit name="family" mode="assign">
      <string>Arimo</string>
    </edit>
    <edit name="rgba" mode="assign">
      <const>none</const>
    </edit>
  </match>

  <match target="pattern">
    <!-- FontConfig doesn't currently provide a well-defined way to turn on
         subpixel positioning.  This is just an arbitrary pattern to use after
         turning subpixel positioning on globally to ensure that we don't have
         issues with our style getting cached for other tests. -->
    <test name="family" compare="eq">
      <string>SubpixelPositioning</string>
    </test>
    <edit name="family" mode="assign">
      <string>Tinos</string>
    </edit>
  </match>

  <match target="pattern">
    <!-- See comments above -->
    <test name="family" compare="eq">
      <string>SubpixelPositioningAhem</string>
    </test>
    <edit name="family" mode="assign">
      <string>ahem</string>
    </edit>
  </match>

  <match target="pattern">
    <test name="family" compare="eq">
      <string>SlightHintedTimesNewRoman</string>
    </test>
    <edit name="family" mode="assign">
      <string>Tinos</string>
    </edit>
    <edit name="hintstyle" mode="assign">
      <const>hintslight</const>
    </edit>
  </match>

  <!-- When we encounter a character that the current font doesn't
       support, gfx::GetFallbackFontForChar() returns the first font
       that does have a glyph for the character. The list of fonts is
       sorted by a pattern that includes the current locale, but doesn't
       include a font family (which means that the fallback font depends
       on the locale but not on the current font).

       DejaVu Sans is commonly the only font that supports some
       characters, such as "⇧", and even when other candidates are
       available, DejaVu Sans is commonly first among them, because of
       the way Fontconfig is ordinarily configured. For example, the
       configuration in the Fonconfig source lists DejaVu Sans under the
       sans-serif generic family, and appends sans-serif to patterns
       that don't already include a generic family (such as the pattern
       in gfx::GetFallbackFontForChar()).

       To get the same fallback font in the layout tests, we could
       duplicate this configuration here, or more directly, simply
       append DejaVu Sans to all patterns. -->
  <match target="pattern">
    <edit name="family" mode="append_last">
      <string>DejaVu Sans</string>
    </edit>
  </match>

</fontconfig>
)";

}  // namespace

void SetUpFontconfig() {
  // TODO(thomasanderson): Use FONTCONFIG_SYSROOT to avoid having to write
  // a new fonts.conf with updated paths.
  std::unique_ptr<Environment> env = Environment::Create();
  if (!env->HasVar("FONTCONFIG_FILE")) {
    // fonts.conf must be generated on-the-fly since it contains absolute paths
    // which may be different if
    //   1. The user moves/renames their build directory (or any parent dirs).
    //   2. The build directory is mapped on a swarming bot at a location
    //      different from the one the buildbot used.
    FilePath dir_module;
    PathService::Get(DIR_MODULE, &dir_module);
    FilePath font_cache = dir_module.Append("fontconfig_caches");
    FilePath test_fonts = dir_module.Append("test_fonts");
    std::string fonts_conf = ReplaceStringPlaceholders(
        kFontsConfTemplate, {font_cache.value(), test_fonts.value()}, nullptr);

    // Write the data to a different file and then atomically rename it to
    // fonts.conf.  This avoids the file being in a bad state when different
    // parallel tests call this function at the same time.
    FilePath fonts_conf_file_temp;
    if(!CreateTemporaryFileInDir(dir_module, &fonts_conf_file_temp))
      CHECK(CreateTemporaryFile(&fonts_conf_file_temp));
    CHECK(
        WriteFile(fonts_conf_file_temp, fonts_conf.c_str(), fonts_conf.size()));
    FilePath fonts_conf_file = dir_module.Append("fonts.conf");
    if (ReplaceFile(fonts_conf_file_temp, fonts_conf_file, nullptr))
      env->SetVar("FONTCONFIG_FILE", fonts_conf_file.value());
    else
      env->SetVar("FONTCONFIG_FILE", fonts_conf_file_temp.value());
  }

  CHECK(FcInit());
}

void TearDownFontconfig() {
  FcFini();
}

}  // namespace base
