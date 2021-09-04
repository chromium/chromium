// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './margin_control.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Coordinate2d} from '../data/coordinate2d.js';
import {CustomMarginsOrientation, Margins, MarginsSetting, MarginsType} from '../data/margins.js';
import {MeasurementSystem} from '../data/measurement_system.js';
import {Size} from '../data/size.js';
import {State} from '../data/state.js';

import {PrintPreviewMarginControlElement} from './margin_control.js';
import {SettingsBehavior, SettingsBehaviorInterface} from './settings_behavior.js';

/**
 * @const {!Map<!CustomMarginsOrientation, string>}
 */
export const MARGIN_KEY_MAP = new Map([
  [CustomMarginsOrientation.TOP, 'marginTop'],
  [CustomMarginsOrientation.RIGHT, 'marginRight'],
  [CustomMarginsOrientation.BOTTOM, 'marginBottom'],
  [CustomMarginsOrientation.LEFT, 'marginLeft']
]);

/** @const {number} */
const MINIMUM_DISTANCE = 72;  // 1 inch


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {SettingsBehaviorInterface}
 */
const PrintPreviewMarginControlContainerElementBase =
    mixinBehaviors([SettingsBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewMarginControlContainerElement extends
    PrintPreviewMarginControlContainerElementBase {
  static get is() {
    return 'print-preview-margin-control-container';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Size} */
      pageSize: {
        type: Object,
        notify: true,
      },

      /** @type {!Margins} */
      documentMargins: {
        type: Object,
        notify: true,
      },

      previewLoaded: Boolean,

      /** @type {?MeasurementSystem} */
      measurementSystem: Object,

      /** @type {!State} */
      state: {
        type: Number,
        observer: 'onStateChanged_',
      },

      /** @private {number} */
      scaleTransform_: {
        type: Number,
        notify: true,
        value: 0,
      },

      /** @private {!Coordinate2d} */
      translateTransform_: {
        type: Object,
        notify: true,
        value: new Coordinate2d(0, 0),
      },

      /** @private {?Size} */
      clipSize_: {
        type: Object,
        notify: true,
        value: null,
      },

      /** @private {boolean} */
      available_: {
        type: Boolean,
        notify: true,
        computed: 'computeAvailable_(previewLoaded, settings.margins.value)',
        observer: 'onAvailableChange_',
      },

      /** @private {boolean} */
      invisible_: {
        type: Boolean,
        reflectToAttribute: true,
        value: true,
      },

      /**
       * @private {!Array<!CustomMarginsOrientation>}
       */
      marginSides_: {
        type: Array,
        notify: true,
        value: [
          CustomMarginsOrientation.TOP,
          CustomMarginsOrientation.RIGHT,
          CustomMarginsOrientation.BOTTOM,
          CustomMarginsOrientation.LEFT,
        ],
      },

      /**
       * String attribute used to set cursor appearance. Possible values:
       * empty (''): No margin control is currently being dragged.
       * 'dragging-horizontal': The left or right control is being dragged.
       * 'dragging-vertical': The top or bottom control is being dragged.
       * @private {string}
       */
      dragging_: {
        type: String,
        reflectToAttribute: true,
        value: '',
      },
    };
  }

  static get observers() {
    return [
      'onMarginSettingsChange_(settings.customMargins.value)',
      'onMediaSizeOrLayoutChange_(' +
          'settings.mediaSize.value, settings.layout.value)',

    ];
  }

  constructor() {
    super();

    /** @private {!Coordinate2d} */
    this.pointerStartPositionInPixels_ = new Coordinate2d(0, 0);

    /** @private {?Coordinate2d} */
    this.marginStartPositionInPixels_ = null;

    /** @private {?boolean} */
    this.resetMargins_ = null;

    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();

    /** @private {boolean} */
    this.textboxFocused_ = false;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeAvailable_() {
    return this.previewLoaded && !!this.clipSize_ &&
        this.getSettingValue('margins') === MarginsType.CUSTOM &&
        !!this.pageSize;
  }

  /** @private */
  onAvailableChange_() {
    if (this.available_ && this.resetMargins_) {
      // Set the custom margins values to the current document margins if the
      // custom margins were reset.
      const newMargins = {};
      for (const side of Object.values(CustomMarginsOrientation)) {
        const key = MARGIN_KEY_MAP.get(side);
        newMargins[key] = this.documentMargins.get(side);
      }
      this.setSetting('customMargins', newMargins);
      this.resetMargins_ = false;
    }
    this.invisible_ = !this.available_;
  }

  /** @private */
  onMarginSettingsChange_() {
    const margins = this.getSettingValue('customMargins');
    if (!margins || margins.marginTop === undefined) {
      // This may be called when print preview model initially sets the
      // settings. It sets custom margins empty by default.
      return;
    }
    this.shadowRoot.querySelectorAll('print-preview-margin-control')
        .forEach(control => {
          const key = MARGIN_KEY_MAP.get(control.side);
          const newValue = margins[key] || 0;
          control.setPositionInPts(newValue);
          control.setTextboxValue(newValue);
        });
  }

  /** @private */
  onMediaSizeOrLayoutChange_() {
    // Reset the custom margins when the paper size changes. Don't do this if
    // it is the first preview.
    if (this.resetMargins_ === null) {
      return;
    }

    this.resetMargins_ = true;
    // Reset custom margins so that the sticky value is not restored for the new
    // paper size.
    this.setSetting('customMargins', {});
  }

  /** @private */
  onStateChanged_() {
    if (this.state === State.READY && this.resetMargins_ === null) {
      // Don't reset margins if there are sticky values. Otherwise, set them
      // to the document margins when the user selects custom margins.
      const margins = this.getSettingValue('customMargins');
      this.resetMargins_ = !margins || margins.marginTop === undefined;
    }
  }

  /**
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  controlsDisabled_() {
    return this.state !== State.READY || this.invisible_;
  }

  /**
   * @param {!CustomMarginsOrientation} orientation
   *     Orientation value to test.
   * @return {boolean} Whether the given orientation is TOP or BOTTOM.
   * @private
   */
  isTopOrBottom_(orientation) {
    return orientation === CustomMarginsOrientation.TOP ||
        orientation === CustomMarginsOrientation.BOTTOM;
  }

  /**
   * @param {!HTMLElement} control Control being repositioned.
   * @param {!Coordinate2d} posInPixels Desired position, in
   *     pixels.
   * @return {number} The new position for the control, in pts. Returns the
   *     position for the dimension that the control operates in, i.e.
   *     x direction for the left/right controls, y direction otherwise.
   * @private
   */
  posInPixelsToPts_(control, posInPixels) {
    const side =
        /** @type {CustomMarginsOrientation} */ (control.side);
    return this.clipAndRoundValue_(
        side,
        control.convertPixelsToPts(
            this.isTopOrBottom_(side) ? posInPixels.y : posInPixels.x));
  }

  /**
   * Moves the position of the given control to the desired position in pts
   * within some constraint minimum and maximum.
   * @param {!HTMLElement} control Control to move.
   * @param {number} posInPts Desired position to move to, in pts. Position is
   *     1 dimensional and represents position in the x direction if control
   * is for the left or right margin, and the y direction otherwise.
   * @private
   */
  moveControlWithConstraints_(control, posInPts) {
    control.setPositionInPts(posInPts);
    control.setTextboxValue(posInPts);
  }

  /**
   * Translates the position of the margin control relative to the pointer
   * position in pixels.
   * @param {!Coordinate2d} pointerPosition New position of
   *     the pointer.
   * @return {!Coordinate2d} New position of the margin control.
   */
  translatePointerToPositionInPixels(pointerPosition) {
    return new Coordinate2d(
        pointerPosition.x - this.pointerStartPositionInPixels_.x +
            this.marginStartPositionInPixels_.x,
        pointerPosition.y - this.pointerStartPositionInPixels_.y +
            this.marginStartPositionInPixels_.y);
  }

  /**
   * Called when the pointer moves in the custom margins component. Moves the
   * dragged margin control.
   * @param {!PointerEvent} event Contains the position of the pointer.
   * @private
   */
  onPointerMove_(event) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (event.target);
    const posInPts = this.posInPixelsToPts_(
        control,
        this.translatePointerToPositionInPixels(
            new Coordinate2d(event.x, event.y)));
    this.moveControlWithConstraints_(control, posInPts);
  }

  /**
   * Called when the pointer is released in the custom margins component.
   * Releases the dragged margin control.
   * @param {!PointerEvent} event Contains the position of the pointer.
   * @private
   */
  onPointerUp_(event) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (event.target);
    this.dragging_ = '';
    const posInPixels = this.translatePointerToPositionInPixels(
        new Coordinate2d(event.x, event.y));
    const posInPts = this.posInPixelsToPts_(control, posInPixels);
    this.moveControlWithConstraints_(control, posInPts);
    this.setMargin_(control.side, posInPts);
    this.updateClippingMask(this.clipSize_);
    this.eventTracker_.remove(control, 'pointercancel');
    this.eventTracker_.remove(control, 'pointerup');
    this.eventTracker_.remove(control, 'pointermove');

    this.fireDragChanged_(false);
  }

  /**
   * @param {boolean} invisible Whether the margin controls should be
   *     invisible.
   */
  setInvisible(invisible) {
    // Ignore changes if the margin controls are not available.
    if (!this.available_) {
      return;
    }

    // Do not set the controls invisible if the user is dragging or focusing
    // the textbox for one of them.
    if (invisible && (this.dragging_ !== '' || this.textboxFocused_)) {
      return;
    }

    this.invisible_ = invisible;
  }

  /**
   * @param {!Event} e Contains information about what control fired the
   *     event.
   * @private
   */
  onTextFocus_(e) {
    this.textboxFocused_ = true;
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (e.target);

    const x = control.offsetLeft;
    const y = control.offsetTop;
    const isTopOrBottom = this.isTopOrBottom_(
        /** @type {!CustomMarginsOrientation} */ (control.side));
    const position = {};
    // Extra padding, in px, to ensure the full textbox will be visible and
    // not just a portion of it. Can't be less than half the width or height
    // of the clip area for the computations below to work.
    const padding = Math.min(
        Math.min(this.clipSize_.width / 2, this.clipSize_.height / 2), 50);

    // Note: clipSize_ gives the current visible area of the margin control
    // container. The offsets of the controls are relative to the origin of
    // this visible area.
    if (isTopOrBottom) {
      // For top and bottom controls, the horizontal position of the box is
      // around halfway across the control's width.
      position.x = Math.min(x + control.offsetWidth / 2 - padding, 0);
      position.x = Math.max(
          x + control.offsetWidth / 2 + padding - this.clipSize_.width,
          position.x);
      // For top and bottom controls, the vertical position of the box is
      // nearly the same as the vertical position of the control.
      position.y = Math.min(y - padding, 0);
      position.y = Math.max(y - this.clipSize_.height + padding, position.y);
    } else {
      // For left and right controls, the horizontal position of the box is
      // nearly the same as the horizontal position of the control.
      position.x = Math.min(x - padding, 0);
      position.x = Math.max(x - this.clipSize_.width + padding, position.x);
      // For top and bottom controls, the vertical position of the box is
      // around halfway up the control's height.
      position.y = Math.min(y + control.offsetHeight / 2 - padding, 0);
      position.y = Math.max(
          y + control.offsetHeight / 2 + padding - this.clipSize_.height,
          position.y);
    }

    this.dispatchEvent(new CustomEvent(
        'text-focus-position',
        {bubbles: true, composed: true, detail: position}));
  }

  /**
   * @param {string} side The margin side. Must be a CustomMarginsOrientation.
   * @param {number} marginValue New value for the margin in points.
   * @private
   */
  setMargin_(side, marginValue) {
    const marginSide =
        /** @type {!CustomMarginsOrientation} */ (side);
    const oldMargins =
        /** @type {MarginsSetting} */ (this.getSettingValue('customMargins'));
    const key = MARGIN_KEY_MAP.get(marginSide);
    if (oldMargins[key] === marginValue) {
      return;
    }
    const newMargins = Object.assign({}, oldMargins);
    newMargins[key] = marginValue;
    this.setSetting('customMargins', newMargins);
  }

  /**
   * @param {string} side The margin side. Must be a CustomMarginsOrientation.
   * @param {number} value The new margin value in points.
   * @return {number} The clipped margin value in points.
   * @private
   */
  clipAndRoundValue_(side, value) {
    const marginSide =
        /** @type {!CustomMarginsOrientation} */ (side);
    if (value < 0) {
      return 0;
    }
    const Orientation = CustomMarginsOrientation;
    let limit = 0;
    const margins = this.getSettingValue('customMargins');
    if (marginSide === Orientation.TOP) {
      limit = this.pageSize.height - margins.marginBottom - MINIMUM_DISTANCE;
    } else if (marginSide === Orientation.RIGHT) {
      limit = this.pageSize.width - margins.marginLeft - MINIMUM_DISTANCE;
    } else if (marginSide === Orientation.BOTTOM) {
      limit = this.pageSize.height - margins.marginTop - MINIMUM_DISTANCE;
    } else {
      assert(marginSide === Orientation.LEFT);
      limit = this.pageSize.width - margins.marginRight - MINIMUM_DISTANCE;
    }
    return Math.round(Math.min(value, limit));
  }

  /**
   * @param {!CustomEvent<number>} e Event containing the new textbox value.
   * @private
   */
  onTextChange_(e) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (e.target);
    control.invalid = false;
    const clippedValue = this.clipAndRoundValue_(control.side, e.detail);
    control.setPositionInPts(clippedValue);
    this.setMargin_(control.side, clippedValue);
  }

  /**
   * @param {!CustomEvent<boolean>} e Event fired when a control's text field
   *     is blurred. Contains information about whether the control is in an
   *     invalid state.
   * @private
   */
  onTextBlur_(e) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (e.target);
    control.setTextboxValue(control.getPositionInPts());
    if (e.detail /* detail is true if the control is in an invalid state */) {
      control.invalid = false;
    }
    this.textboxFocused_ = false;
  }

  /**
   * @param {!PointerEvent} e Fired when pointerdown occurs on a margin
   *     control.
   * @private
   */
  onPointerDown_(e) {
    const control =
        /** @type {!PrintPreviewMarginControlElement} */ (e.target);
    if (!control.shouldDrag(e)) {
      return;
    }

    this.pointerStartPositionInPixels_ = new Coordinate2d(e.x, e.y);
    this.marginStartPositionInPixels_ =
        new Coordinate2d(control.offsetLeft, control.offsetTop);
    this.dragging_ = this.isTopOrBottom_(
                         /** @type {CustomMarginsOrientation} */
                         (control.side)) ?
        'dragging-vertical' :
        'dragging-horizontal';
    this.eventTracker_.add(
        control, 'pointercancel',
        e => this.onPointerUp_(/** @type {!PointerEvent} */ (e)));
    this.eventTracker_.add(
        control, 'pointerup',
        e => this.onPointerUp_(/** @type {!PointerEvent} */ (e)));
    this.eventTracker_.add(
        control, 'pointermove',
        e => this.onPointerMove_(/** @type {!PointerEvent} */ (e)));
    control.setPointerCapture(e.pointerId);

    this.fireDragChanged_(true);
  }

  /**
   * @param {boolean} dragChanged
   * @private
   */
  fireDragChanged_(dragChanged) {
    this.dispatchEvent(new CustomEvent(
        'margin-drag-changed',
        {bubbles: true, composed: true, detail: dragChanged}));
  }

  /**
   * Set display:none after the opacity transition for the controls is done.
   * @private
   */
  onTransitionEnd_() {
    if (this.invisible_) {
      this.style.display = 'none';
    }
  }

  /**
   * Updates the translation transformation that translates pixel values in
   * the space of the HTML DOM.
   * @param {Coordinate2d} translateTransform Updated value of
   *     the translation transformation.
   */
  updateTranslationTransform(translateTransform) {
    if (!translateTransform.equals(this.translateTransform_)) {
      this.translateTransform_ = translateTransform;
    }
  }

  /**
   * Updates the scaling transform that scales pixels values to point values.
   * @param {number} scaleTransform Updated value of the scale transform.
   */
  updateScaleTransform(scaleTransform) {
    if (scaleTransform !== this.scaleTransform_) {
      this.scaleTransform_ = scaleTransform;
    }
  }

  /**
   * Clips margin controls to the given clip size in pixels.
   * @param {Size} clipSize Size to clip the margin controls to.
   */
  updateClippingMask(clipSize) {
    if (!clipSize) {
      return;
    }
    this.clipSize_ = clipSize;
    this.notifyPath('clipSize_');
  }
}

customElements.define(
    PrintPreviewMarginControlContainerElement.is,
    PrintPreviewMarginControlContainerElement);
