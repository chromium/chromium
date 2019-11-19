// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './icons.js';
import './viewer-page-selector.js';
import './viewer-toolbar-dropdown.js';

// <if expr="chromeos">
import './viewer-pen-options.js';
// </if>

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Bookmark} from './viewer-bookmark.js';

Polymer({
  is: 'viewer-pdf-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * Whether annotation mode can be entered. This would be false if for
     * example the PDF is encrypted or password protected. Note, this is
     * true regardless of whether the feature flag is enabled.
     */
    annotationAvailable: {
      type: Boolean,
      value: true,
    },

    /** Whether the viewer is currently in annotation mode. */
    annotationMode: {
      type: Boolean,
      notify: true,
      value: false,
      reflectToAttribute: true,
    },

    /** @type {?AnnotationTool} */
    annotationTool: {
      type: Object,
      value: null,
      notify: true,
    },

    /**
     * Tree of PDF bookmarks (empty if the document has no bookmarks).
     * @type {!Array<!Bookmark>}
     */
    bookmarks: {
      type: Array,
      value: () => [],
    },

    canRedoAnnotation: {
      type: Boolean,
      value: false,
    },

    canUndoAnnotation: {
      type: Boolean,
      value: false,
    },

    /** The number of pages in the PDF document. */
    docLength: Number,

    /** The title of the PDF document. */
    docTitle: String,

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

    /** The number of the page being viewed (1-based). */
    pageNo: Number,

    /** Whether the PDF Annotations feature is enabled. */
    pdfAnnotationsEnabled: {
      type: Boolean,
      value: false,
    },

    /** Whether the Printing feature is enabled. */
    printingEnabled: {
      type: Boolean,
      value: false,
    },

    strings: Object,
  },

  /** @type {?Object} */
  animation_: null,

  /**
   * @param {number} newProgress
   * @param {number} oldProgress
   * @private
   */
  loadProgressChanged_: function(newProgress, oldProgress) {
    const loaded = newProgress >= 100;
    const progressReset = newProgress < oldProgress;
    if (progressReset || loaded) {
      this.$.pageselector.classList.toggle('invisible', !loaded);
      this.$.buttons.classList.toggle('invisible', !loaded);
      this.$.progress.style.opacity = loaded ? 0 : 1;
      this.$['annotations-bar'].hidden = !loaded || !this.annotationMode;
    }
  },

  hide: function() {
    if (this.opened && !this.shouldKeepOpen()) {
      this.toggleVisibility();
    }
  },

  show: function() {
    if (!this.opened) {
      this.toggleVisibility();
    }
  },

  toggleVisibility: function() {
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

  selectPageNumber: function() {
    this.$.pageselector.select();
  },

  /** @return {boolean} Whether the toolbar should be kept open. */
  shouldKeepOpen: function() {
    return this.$.bookmarks.dropdownOpen || this.loadProgress < 100 ||
        this.$.pageselector.isActive() || this.annotationMode;
  },

  /** @return {boolean} Whether a dropdown was open and was hidden. */
  hideDropdowns: function() {
    let result = false;
    if (this.$.bookmarks.dropdownOpen) {
      this.$.bookmarks.toggleDropdown();
      result = true;
    }
    if (this.$.pen.dropdownOpen) {
      this.$.pen.toggleDropdown();
      result = true;
    }
    if (this.$.highlighter.dropdownOpen) {
      this.$.highlighter.toggleDropdown();
      result = true;
    }
    return result;
  },

  /** @param {number} lowerBound */
  setDropdownLowerBound: function(lowerBound) {
    this.$.bookmarks.lowerBound = lowerBound;
  },

  rotateRight: function() {
    this.fire('rotate-right');
  },

  download: function() {
    this.fire('save');
  },

  print: function() {
    this.fire('print');
  },

  undo: function() {
    this.fire('undo');
  },

  redo: function() {
    this.fire('redo');
  },

  toggleAnnotation: function() {
    this.annotationMode = !this.annotationMode;
    if (this.annotationMode) {
      // Select pen tool when entering annotation mode.
      this.updateAnnotationTool_(/** @type {!HTMLElement} */ (this.$.pen));
    }
    this.dispatchEvent(new CustomEvent('annotation-mode-toggled', {
      detail: {
        value: this.annotationMode,
      },
    }));
  },

  /**
   * @param {!Event} e
   * @private
   */
  annotationToolClicked_: function(e) {
    this.updateAnnotationTool_(/** @type {!HTMLElement} */ (e.currentTarget));
  },

  /**
   * @param {!Event} e
   * @private
   */
  annotationToolOptionChanged_: function(e) {
    const element = e.currentTarget.parentElement;
    if (!this.annotationTool || element.id != this.annotationTool.tool) {
      return;
    }
    this.updateAnnotationTool_(e.currentTarget.parentElement);
  },

  /**
   * @param {!HTMLElement} element
   * @private
   */
  updateAnnotationTool_: function(element) {
    const tool = element.id;
    const options = element.querySelector('viewer-pen-options') || {
      selectedSize: 1,
      selectedColor: null,
    };
    const attributeStyleMap = element.attributeStyleMap;
    attributeStyleMap.set('--pen-tip-fill', options.selectedColor);
    attributeStyleMap.set(
        '--pen-tip-border',
        options.selectedColor == '#000000' ? 'currentcolor' :
                                             options.selectedColor);
    this.annotationTool = {
      tool: tool,
      size: options.selectedSize,
      color: options.selectedColor,
    };
  },

  /**
   * @param {string} toolName
   * @return {boolean} Whether the annotation tool is using tool |toolName|.
   * @private
   */
  isAnnotationTool_: function(toolName) {
    return !!this.annotationTool && this.annotationTool.tool === toolName;
  },
});
