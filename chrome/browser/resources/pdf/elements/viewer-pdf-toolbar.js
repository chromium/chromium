// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './icons.js';
import './shared-css.js';
import './viewer-bookmark.js';
import './viewer-download-controls.js';
import './viewer-page-selector.js';
import './viewer-toolbar-dropdown.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from '../bookmark_type.js';
// <if expr="chromeos">
import {ViewerAnnotationsBarElement} from './viewer-annotations-bar.js';
// </if>

Polymer({
  is: 'viewer-pdf-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Whether annotation mode can be entered. This would be false if for
     * example the PDF is encrypted or password protected. Note, this is
     * true regardless of whether the feature flag is enabled.
     */
    annotationAvailable: Boolean,

    /** Whether the viewer is currently in annotation mode. */
    annotationMode: {
      type: Boolean,
      notify: true,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Tree of PDF bookmarks (empty if the document has no bookmarks).
     * @type {!Array<!Bookmark>}
     */
    bookmarks: {
      type: Array,
      value: () => [],
    },

    docLength: Number,

    /** The title of the PDF document. */
    docTitle: String,

    hasEdits: Boolean,

    hasEnteredAnnotationMode: Boolean,

    isFormFieldFocused: Boolean,

    /** The current loading progress of the PDF document (0 - 100). */
    loadProgress: {
      type: Number,
      observer: 'loadProgressChanged_',
    },

    /** Whether the toolbar is opened and visible. */
    opened: {
      type: Boolean,
      value: true,
    },

    pageNo: Number,

    /** Whether the PDF Annotations feature is enabled. */
    pdfAnnotationsEnabled: Boolean,

    /** Whether the Printing feature is enabled. */
    printingEnabled: Boolean,

    /** Whether the PDF Form save feature is enabled. */
    pdfFormSaveEnabled: Boolean,
  },

  /** @type {?Object} */
  animation_: null,

  /**
   * @param {number} newProgress
   * @param {number} oldProgress
   * @private
   */
  loadProgressChanged_(newProgress, oldProgress) {
    const loaded = newProgress >= 100;
    const progressReset = newProgress < oldProgress;
    if (progressReset || loaded) {
      this.$.pageselector.classList.toggle('invisible', !loaded);
      this.$.buttons.classList.toggle('invisible', !loaded);
      this.$.progress.style.opacity = loaded ? 0 : 1;
      // <if expr="chromeos">
      this.$$('viewer-annotations-bar').hidden =
          !loaded || !this.annotationMode;
      // </if>
    }
  },

  hide() {
    if (this.opened && !this.shouldKeepOpen()) {
      this.toggleVisibility();
    }
  },

  show() {
    if (!this.opened) {
      this.toggleVisibility();
    }
  },

  toggleVisibility() {
    this.opened = !this.opened;

    // We keep a handle on the animation in order to cancel the filling
    // behavior of previous animations.
    if (this.animation_) {
      this.animation_.cancel();
    }

    if (this.opened) {
      this.animation_ = this.animate(
          [{transform: 'translateY(-100%)'}, {transform: 'translateY(0%)'}], {
            duration: 250,
            easing: 'cubic-bezier(0, 0, 0.2, 1)',
            fill: 'forwards',
          });
    } else {
      this.animation_ = this.animate(
          [{transform: 'translateY(0%)'}, {transform: 'translateY(-100%)'}], {
            duration: 250,
            easing: 'cubic-bezier(0.4, 0, 1, 1)',
            fill: 'forwards',
          });
    }
  },

  selectPageNumber() {
    this.$.pageselector.select();
  },

  /** @return {boolean} Whether the toolbar should be kept open. */
  shouldKeepOpen() {
    return this.$.bookmarks.dropdownOpen || this.loadProgress < 100 ||
        this.$.pageselector.isActive() || this.annotationMode ||
        this.$.downloads.isMenuOpen();
  },

  /** @return {boolean} Whether a dropdown was open and was hidden. */
  hideDropdowns() {
    let result = false;
    if (this.$.bookmarks.dropdownOpen) {
      this.$.bookmarks.toggleDropdown();
      result = true;
    }
    if (this.$.downloads.isMenuOpen()) {
      this.$.downloads.closeMenu();
      result = true;
    }
    // <if expr="chromeos">
    const annotationBar = /** @type {!ViewerAnnotationsBarElement} */ (
        this.$$('viewer-annotations-bar'));
    if (annotationBar.hasOpenDropdown()) {
      annotationBar.closeDropdowns();
      result = true;
    }
    // </if>
    return result;
  },

  /** @param {number} lowerBound */
  setDropdownLowerBound(lowerBound) {
    this.$.bookmarks.lowerBound = lowerBound;
  },

  rotateRight() {
    this.fire('rotate-right');
  },

  print() {
    this.fire('print');
  },

  // <if expr="chromeos">
  toggleAnnotation() {
    this.annotationMode = !this.annotationMode;
    this.dispatchEvent(new CustomEvent(
        'annotation-mode-toggled', {detail: this.annotationMode}));
  },
  // </if>
});
