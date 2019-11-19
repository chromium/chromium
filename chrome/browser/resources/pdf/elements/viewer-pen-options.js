// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';

import {beforeNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const colors = [
  // row 1
  {name: 'annotationColorBlack', color: '#000000'},
  {name: 'annotationColorRed', color: '#ff5252'},
  {name: 'annotationColorYellow', color: '#ffbc00'},
  {name: 'annotationColorGreen', color: '#00c853'},
  {name: 'annotationColorCyan', color: '#00b0ff'},
  {name: 'annotationColorPurple', color: '#d500f9'},
  {name: 'annotationColorBrown', color: '#8d6e63'},
  // row 2
  {name: 'annotationColorWhite', color: '#fafafa', outline: true},
  {name: 'annotationColorCrimson', color: '#a52714'},
  {name: 'annotationColorAmber', color: '#ee8100'},
  {name: 'annotationColorAvocadoGreen', color: '#558b2f'},
  {name: 'annotationColorCobaltBlue', color: '#01579b'},
  {name: 'annotationColorDeepPurple', color: '#8e24aa'},
  {name: 'annotationColorDarkBrown', color: '#4e342e'},
  // row 3
  {name: 'annotationColorDarkGrey', color: '#90a4ae'},
  {name: 'annotationColorHotPink', color: '#ff4081'},
  {name: 'annotationColorOrange', color: '#ff6e40'},
  {name: 'annotationColorLime', color: '#aeea00'},
  {name: 'annotationColorBlue', color: '#304ffe'},
  {name: 'annotationColorViolet', color: '#7c4dff'},
  {name: 'annotationColorTeal', color: '#1de9b6'},
  // row 4
  {name: 'annotationColorLightGrey', color: '#cfd8dc'},
  {name: 'annotationColorLightPink', color: '#f8bbd0'},
  {name: 'annotationColorLightOrange', color: '#ffccbc'},
  {name: 'annotationColorLightGreen', color: '#f0f4c3'},
  {name: 'annotationColorLightBlue', color: '#9fa8da'},
  {name: 'annotationColorLavender', color: '#d1c4e9'},
  {name: 'annotationColorLightTeal', color: '#b2dfdb'},
];

const sizes = [
  {name: 'annotationSize1', size: 0},
  {name: 'annotationSize2', size: 0.1429},
  {name: 'annotationSize3', size: 0.2857},
  {name: 'annotationSize4', size: 0.4286},
  {name: 'annotationSize8', size: 0.5714},
  {name: 'annotationSize12', size: 0.7143},
  {name: 'annotationSize16', size: 0.8571},
  {name: 'annotationSize20', size: 1},
];

/**
 * Displays a set of radio buttons to select from
 * a predefined list of colors and sizes.
 */
Polymer({
  is: 'viewer-pen-options',

  _template: html`{__html_template__}`,

  properties: {
    expanded_: {
      type: Boolean,
      value: false,
    },
    selectedSize: {
      type: Number,
      value: 0.250,
      notify: true,
    },
    selectedColor: {
      type: String,
      value: '#000000',
      notify: true,
    },
    sizes_: {
      type: Array,
      value: sizes,
    },
    colors_: {
      type: Array,
      value: colors,
    },
    strings: Object,
  },

  /** @type {Array<!Animation>} */
  expandAnimations_: null,

  /** @param {Event} e */
  sizeChanged_: function(e) {
    this.selectedSize = Number(e.target.value);
  },

  /** @param {Event} e */
  colorChanged_: function(e) {
    this.selectedColor = e.target.value;
  },

  /** @private */
  toggleExpanded_: function() {
    this.expanded_ = !this.expanded_;
    this.updateExpandedState_();
  },

  /** @private */
  updateExpandedStateAndFinishAnimations_: function() {
    this.updateExpandedState_();
    for (const animation of /** @type {!Array<!Animation>} */ (
             this.expandAnimations_)) {
      animation.finish();
    }
  },

  /** @override */
  attached: function() {
    beforeNextRender(this, () => {
      this.updateExpandedStateAndFinishAnimations_();
    });
  },

  /**
   * Updates the state of the UI to reflect the current value of `expanded`.
   * Starts or reverses animations and enables/disable controls.
   */
  updateExpandedState_: function() {
    const colors = this.$.colors;
    if (!this.expandAnimations_) {
      const separator = this.$.separator;
      const expand = this.$.expand;
      this.expandAnimations_ = [
        colors.animate(
            [
              {height: '32px'},
              {height: '188px'},
            ],
            {
              easing: 'ease-in-out',
              duration: 250,
              fill: 'both',
            }),
        separator.animate(
            [
              {opacity: 0},
              {opacity: 1},
            ],
            {
              easing: 'ease-in-out',
              duration: 250,
              fill: 'both',
            }),
        expand.animate(
            [
              {transform: 'rotate(0deg)'},
              {transform: 'rotate(180deg)'},
            ],
            {
              easing: 'ease-in-out',
              duration: 250,
              fill: 'forwards',
            }),
      ];
    }
    for (const animation of this.expandAnimations_) {
      // TODO(dstockwell): Ideally we would just set playbackRate,
      // but there appears to be a web-animations bug that
      // results in the animation getting stuck in the 'pending'
      // state sometimes. See crbug.com/938857
      const currentTime = animation.currentTime;
      animation.cancel();
      animation.playbackRate = this.expanded_ ? 1 : -1;
      animation.currentTime = currentTime;
      animation.play();
    }
    for (const input of colors.querySelectorAll('input:nth-child(n+8)')) {
      if (this.expanded_) {
        input.removeAttribute('disabled');
      } else {
        input.setAttribute('disabled', '');
      }
    }
  },

  /**
   * Used to determine equality in computed bindings.
   *
   * @param {*} a
   * @param {*} b
   */
  equal_: function(a, b) {
    return a == b;
  },

  /**
   * Used to lookup a string in a computed binding.
   *
   * @param {Object} strings
   * @param {string} name
   * @return {string}
   */
  lookup_: function(strings, name) {
    return strings ? strings[name] : '';
  },

  /**
   * Used to remove focus when clicking or tapping on a styled input
   * element. This is a workaround until we can use the :focus-visible
   * pseudo selector.
   */
  blurOnPointerDown(e) {
    const target = e.target;
    setTimeout(() => target.blur(), 0);
  },
});
