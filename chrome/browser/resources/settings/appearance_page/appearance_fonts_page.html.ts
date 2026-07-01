// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsAppearanceFontsPageElement} from './appearance_fonts_page.js';

export function getHtml(this: SettingsAppearanceFontsPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<settings-subpage page-title="$i18n{customizeFonts}"
    route-path="${this.routePath}">
  <div class="cr-row first">
    <div class="flex cr-padded-text" aria-hidden="true">
      $i18n{fontSize}
    </div>
    <settings-slider id="sizeSlider"
        pref-key="webkit.webprefs.default_font_size"
        .ticks="${this.fontSizeRange_}"
        label-aria="$i18n{fontSize}"
        label-min="$i18n{tiny}" label-max="$i18n{huge}">
    </settings-slider>
  </div>
  <div class="cr-row">
    <div class="flex cr-padded-text" aria-hidden="true">$i18n{minimumFont}</div>
    <div id="minimumSize">
      <settings-slider pref-key="webkit.webprefs.minimum_font_size"
          .ticks="${this.minimumFontSizeRange_}"
          label-aria="$i18n{minimumFont}"
          label-min="$i18n{tiny}" label-max="$i18n{huge}">
      </settings-slider>
      <div id="minimumSizeFontPreview"
          style="font-size:${this.getMinimumFontSize_()}px;
              font-family:'${this.standardFontPref_?.value || ''}';"
          ?hidden="${this.getMinimumSizeHidden_()}">
        ${this.getMinimumFontSize_()}:
        $i18n{quickBrownFox}
      </div>
    </div>
  </div>
  <div class="cr-row" aria-hidden="true">
    <h2>$i18n{standardFont}</h2>
  </div>
  <div class="list-frame">
    <div class="list-item">
      <settings-dropdown-menu class="start"
          label="$i18n{standardFont}"
          pref-key="webkit.webprefs.fonts.standard.Zyyy"
          .menuOptions="${this.fontOptions_}">
      </settings-dropdown-menu>
    </div>
    <div id="standardFontPreview" class="list-item cr-padded-text"
        style="font-size:${this.defaultFontSizePref_?.value || 0}px;
            font-family:'${this.standardFontPref_?.value || ''}';">
      ${this.defaultFontSizePref_?.value || 0}:
      $i18n{quickBrownFox}
    </div>
  </div>
  <div class="cr-row" aria-hidden="true">
    <h2>$i18n{serifFont}</h2>
  </div>
  <div class="list-frame">
    <div class="list-item">
      <settings-dropdown-menu class="start"
          label="$i18n{serifFont}"
          pref-key="webkit.webprefs.fonts.serif.Zyyy"
          .menuOptions="${this.fontOptions_}">
      </settings-dropdown-menu>
    </div>
    <div id="serifFontPreview" class="list-item cr-padded-text"
        style="font-size:${this.defaultFontSizePref_?.value || 0}px;
            font-family:'${this.serifFontPref_?.value || ''}';">
      ${this.defaultFontSizePref_?.value || 0}:
      $i18n{quickBrownFox}
    </div>
  </div>
  <div class="cr-row" aria-hidden="true">
    <h2>$i18n{sansSerifFont}</h2>
  </div>
  <div class="list-frame">
    <div class="list-item">
      <settings-dropdown-menu class="start"
          label="$i18n{sansSerifFont}"
          pref-key="webkit.webprefs.fonts.sansserif.Zyyy"
          .menuOptions="${this.fontOptions_}">
      </settings-dropdown-menu>
    </div>
    <div id="sansSerifFontPreview" class="list-item cr-padded-text"
        style="font-size:${this.defaultFontSizePref_?.value || 0}px;
            font-family:'${this.sansSerifFontPref_?.value || ''}';">
      ${this.defaultFontSizePref_?.value || 0}:
      $i18n{quickBrownFox}
    </div>
  </div>
  <div class="cr-row" aria-hidden="true">
    <h2>$i18n{fixedWidthFont}</h2>
  </div>
  <div class="list-frame">
    <div class="list-item">
      <settings-dropdown-menu class="start"
          label="$i18n{fixedWidthFont}"
          pref-key="webkit.webprefs.fonts.fixed.Zyyy"
          .menuOptions="${this.fontOptions_}">
      </settings-dropdown-menu>
    </div>
    <div id="fixedFontPreview" class="list-item cr-padded-text"
        style="font-size: ${this.defaultFixedFontSizePref_?.value || 0}px;
            font-family: '${this.fontFamilyValueForFixed_()}';">
      ${this.defaultFixedFontSizePref_?.value || 0}:
      $i18n{quickBrownFox}
    </div>
  </div>
  <div class="cr-row" aria-hidden="true">
    <h2>$i18n{mathFont}</h2>
  </div>
  <div class="list-frame">
    <div class="list-item">
      <settings-dropdown-menu class="start"
          label="$i18n{mathFont}"
          pref-key="webkit.webprefs.fonts.math.Zyyy"
          .menuOptions="${this.fontOptions_}">
      </settings-dropdown-menu>
    </div>
    <!-- A text preview like quickBrownFox is not really helpful for
         mathematical fonts. Not only it's desired to show special math
         characters but also to demo advanced features involving the
         OpenType MATH table such as big/stretchy operators or special
         layout constants. This is what the formula below tries to do. -->
    <div id="mathFontPreview" class="list-item cr-padded-text"
        style="font-size:${this.defaultFontSizePref_?.value || 0}px;
            font-family:'${this.mathFontPref_?.value || ''}';">
      ${this.defaultFontSizePref_?.value || 0}:
      <math style="font: inherit;" displaystyle="true">
        <mrow>
          <msqrt>
            <mrow>
              <munderover>
                <mo>∑</mo>
                <mrow>
                  <mi>n</mi>
                  <mo>=</mo>
                  <mn>1</mn>
                </mrow>
                <mn>∞</mn>
              </munderover>
              <mfrac>
                <mn>10</mn>
                <msup>
                  <mi>n</mi>
                  <mn>4</mn>
                </msup>
              </mfrac>
            </mrow>
          </msqrt>
          <mo>=</mo>
          <mrow>
            <msubsup>
              <mo>∫</mo>
              <mn>0</mn>
              <mn>∞</mn>
            </msubsup>
            <mfrac>
              <mrow>
                <mn>2</mn>
                <mi>x</mi>
                <mrow>
                  <mi>d</mi>
                  <mi>x</mi>
                </mrow>
              </mrow>
              <mrow>
                <msup>
                  <mi>e</mi>
                  <mi>x</mi>
                </msup>
                <mo>−</mo>
                <mn>1</mn>
              </mrow>
            </mfrac>
          </mrow>
          <mo>=</mo>
          <mfrac>
            <msup>
              <mi>π</mi>
              <mn>2</mn>
            </msup>
            <mn>3</mn>
          </mfrac>
          <mo>∊</mo>
          <mi>ℝ</mi>
        </mrow>
      </math>
    </div>
  </div>
</settings-subpage>
<!--_html_template_end_-->`;
  // clang-format on
}
