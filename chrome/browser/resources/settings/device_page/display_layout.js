// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'display-layout' presents a visual representation of the layout of one or
 * more displays and allows them to be arranged.
 */

(function() {

/** @type {number} */ const MIN_VISUAL_SCALE = .01;

Polymer({
  is: 'display-layout',

  behaviors: [
    Polymer.IronResizableBehavior,
    DragBehavior,
    LayoutBehavior,
  ],

  properties: {
    /**
     * Array of displays.
     * @type {!Array<!chrome.system.display.DisplayUnitInfo>}
     */
    displays: Array,

    /** @type {!chrome.system.display.DisplayUnitInfo|undefined} */
    selectedDisplay: Object,

    /**
     * The ratio of the display area div (in px) to DisplayUnitInfo.bounds.
     * @type {number}
     */
    visualScale: {
      type: Number,
      value: 1,
    },

    /**
     * Ids for mirroring destination displays.
     * @type {!Array<string>|undefined}
     * @private
     */
    mirroringDestinationIds_: Array,
  },

  /** @private {!{left: number, top: number}} */
  visualOffset_: {left: 0, top: 0},

  /** @override */
  detached: function() {
    this.initializeDrag(false);
  },

  /**
   * Called explicitly when |this.displays| and their associated |this.layouts|
   * have been fetched from chrome.
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {!Array<!chrome.system.display.DisplayLayout>} layouts
   * @param {!Array<string>} mirroringDestinationIds
   */
  updateDisplays: function(displays, layouts, mirroringDestinationIds) {
    this.displays = displays;
    this.layouts = layouts;
    this.mirroringDestinationIds_ = mirroringDestinationIds;

    this.initializeDisplayLayout(displays, layouts);

    const self = this;
    const retry = 100;  // ms
    function tryCalcVisualScale() {
      if (!self.calculateVisualScale_())
        setTimeout(tryCalcVisualScale, retry);
    }
    tryCalcVisualScale();

    this.initializeDrag(
        !this.mirroring, this.$.displayArea, this.onDrag_.bind(this));
  },

  /**
   * Calculates the visual offset and scale for the display area
   * (i.e. the ratio of the display area div size to the area required to
   * contain the DisplayUnitInfo bounding boxes).
   * @return {boolean} Whether the calculation was successful.
   * @private
   */
  calculateVisualScale_: function() {
    const displayAreaDiv = this.$.displayArea;
    if (!displayAreaDiv || !displayAreaDiv.offsetWidth || !this.displays ||
        !this.displays.length) {
      return false;
    }

    let display = this.displays[0];
    let bounds = this.getCalculatedDisplayBounds(display.id);
    const boundsBoundingBox = {
      left: bounds.left,
      right: bounds.left + bounds.width,
      top: bounds.top,
      bottom: bounds.top + bounds.height,
    };
    let maxWidth = bounds.width;
    let maxHeight = bounds.height;
    for (let i = 1; i < this.displays.length; ++i) {
      display = this.displays[i];
      bounds = this.getCalculatedDisplayBounds(display.id);
      boundsBoundingBox.left = Math.min(boundsBoundingBox.left, bounds.left);
      boundsBoundingBox.right =
          Math.max(boundsBoundingBox.right, bounds.left + bounds.width);
      boundsBoundingBox.top = Math.min(boundsBoundingBox.top, bounds.top);
      boundsBoundingBox.bottom =
          Math.max(boundsBoundingBox.bottom, bounds.top + bounds.height);
      maxWidth = Math.max(maxWidth, bounds.width);
      maxHeight = Math.max(maxHeight, bounds.height);
    }

    // Create a margin around the bounding box equal to the size of the
    // largest displays.
    const boundsWidth = boundsBoundingBox.right - boundsBoundingBox.left;
    const boundsHeight = boundsBoundingBox.bottom - boundsBoundingBox.top;

    // Calculate the scale.
    const horizontalScale =
        displayAreaDiv.offsetWidth / (boundsWidth + maxWidth * 2);
    const verticalScale =
        displayAreaDiv.offsetHeight / (boundsHeight + maxHeight * 2);
    const scale = Math.min(horizontalScale, verticalScale);

    // Calculate the offset.
    this.visualOffset_.left =
        ((displayAreaDiv.offsetWidth - (boundsWidth * scale)) / 2) -
        boundsBoundingBox.left * scale;
    this.visualOffset_.top =
        ((displayAreaDiv.offsetHeight - (boundsHeight * scale)) / 2) -
        boundsBoundingBox.top * scale;

    // Update the scale which will trigger calls to getDivStyle_.
    this.visualScale = Math.max(MIN_VISUAL_SCALE, scale);

    return true;
  },

  /**
   * @param {string} id
   * @param {!chrome.system.display.Bounds} displayBounds
   * @param {number} visualScale
   * @param {number=} opt_offset
   * @return {string} The style string for the div.
   * @private
   */
  getDivStyle_: function(id, displayBounds, visualScale, opt_offset) {
    // This matches the size of the box-shadow or border in CSS.
    /** @type {number} */ const BORDER = 1;
    /** @type {number} */ const MARGIN = 4;
    /** @type {number} */ const OFFSET = opt_offset || 0;
    /** @type {number} */ const PADDING = 3;
    const bounds = this.getCalculatedDisplayBounds(id, true /* notest */);
    if (!bounds)
      return '';
    const height = Math.round(bounds.height * this.visualScale) - BORDER * 2 -
        MARGIN * 2 - PADDING * 2;
    const width = Math.round(bounds.width * this.visualScale) - BORDER * 2 -
        MARGIN * 2 - PADDING * 2;
    const left = OFFSET +
        Math.round(this.visualOffset_.left + (bounds.left * this.visualScale));
    const top = OFFSET +
        Math.round(this.visualOffset_.top + (bounds.top * this.visualScale));
    return 'height: ' + height + 'px; width: ' + width + 'px;' +
        ' left: ' + left + 'px; top: ' + top + 'px';
  },

  /**
   * @param {number} mirroringDestinationIndex
   * @param {number} mirroringDestinationDisplayNum
   * @param {!Array<!chrome.system.display.DisplayUnitInfo>} displays
   * @param {number} visualScale
   * @return {string} The style string for the mirror div.
   * @private
   */
  getMirrorDivStyle_: function(
      mirroringDestinationIndex, mirroringDestinationDisplayNum, displays,
      visualScale) {
    // All destination displays have the same bounds as the mirroring source
    // display, but we add a little offset to each destination display's bounds
    // so that they can be distinguished from each other in the layout.
    return this.getDivStyle_(
        displays[0].id, displays[0].bounds, visualScale,
        (mirroringDestinationDisplayNum - mirroringDestinationIndex) * -4);
  },

  /**
   * @param {boolean} mirroring
   * @param {string} displayName
   * @param {string} mirroringName
   * @return {string}
   * @private
   */
  getDisplayName_: function(mirroring, displayName, mirroringName) {
    return mirroring ? mirroringName : displayName;
  },

  /**
   * @param {!chrome.system.display.DisplayUnitInfo} display
   * @param {!chrome.system.display.DisplayUnitInfo} selectedDisplay
   * @return {boolean}
   * @private
   */
  isSelected_: function(display, selectedDisplay) {
    return display.id == selectedDisplay.id;
  },

  /**
   * @param {!{model: !{item: !chrome.system.display.DisplayUnitInfo},
   *     target: !HTMLDivElement}} e
   * @private
   */
  onSelectDisplayTap_: function(e) {
    this.fire('select-display', e.model.item.id);
    // Force active in case the selected display was clicked.
    // TODO(dpapad): Ask @stevenjb, why are we setting 'active' on a div?
    e.target.active = true;
  },

  /**
   * @param {string} id
   * @param {?DragPosition} amount
   */
  onDrag_: function(id, amount) {
    id = id.substr(1);  // Skip prefix

    let newBounds;
    if (!amount) {
      this.finishUpdateDisplayBounds(id);
      newBounds = this.getCalculatedDisplayBounds(id);
    } else {
      // Make sure the dragged display is also selected.
      if (id != this.selectedDisplay.id)
        this.fire('select-display', id);

      const calculatedBounds = this.getCalculatedDisplayBounds(id);
      newBounds =
          /** @type {chrome.system.display.Bounds} */ (
              Object.assign({}, calculatedBounds));
      newBounds.left += Math.round(amount.x / this.visualScale);
      newBounds.top += Math.round(amount.y / this.visualScale);

      if (this.displays.length >= 2)
        newBounds = this.updateDisplayBounds(id, newBounds);
    }
    const left =
        this.visualOffset_.left + Math.round(newBounds.left * this.visualScale);
    const top =
        this.visualOffset_.top + Math.round(newBounds.top * this.visualScale);
    const div = this.$$('#_' + id);
    div.style.left = '' + left + 'px';
    div.style.top = '' + top + 'px';
  },

});

})();
