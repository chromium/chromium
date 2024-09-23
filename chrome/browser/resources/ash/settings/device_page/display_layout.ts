// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'display-layout' presents a visual representation of the layout of one or
 * more displays and allows them to be arranged.
 */

import '../settings_shared.css.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronResizableBehavior} from 'chrome://resources/polymer/v3_0/iron-resizable-behavior/iron-resizable-behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {Constructor} from '../common/types.js';

import {DevicePageBrowserProxy, DevicePageBrowserProxyImpl} from './device_page_browser_proxy.js';
import {getTemplate} from './display_layout.html.js';
import {LayoutMixin, LayoutMixinInterface, Position} from './layout_mixin.js';

import Bounds = chrome.system.display.Bounds;
import DisplayLayout = chrome.system.display.DisplayLayout;
import DisplayUnitInfo = chrome.system.display.DisplayUnitInfo;

/**
 * Container for DisplayUnitInfo.  Mostly here to make the DisplaySelectEvent
 * typedef more readable.
 */
interface InfoItem {
  item: DisplayUnitInfo;
}

/**
 * Required member fields for events which select displays.
 */
interface DisplaySelectEvent {
  model: InfoItem;
  target: HTMLElement;
}

const MIN_VISUAL_SCALE = .01;

export interface DisplayLayoutElement {
  $: {
    displayArea: HTMLElement,
  };
}

const DisplayLayoutElementBase =
    mixinBehaviors(
        [IronResizableBehavior], LayoutMixin(I18nMixin(PolymerElement))) as
    Constructor<PolymerElement&I18nMixinInterface&LayoutMixinInterface>;

export class DisplayLayoutElement extends DisplayLayoutElementBase {
  static get is() {
    return 'display-layout';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Array of displays.
       */
      displays: Array,

      selectedDisplay: Object,

      /**
       * The ratio of the display area div (in px) to DisplayUnitInfo.bounds.
       */
      visualScale: {
        type: Number,
        value: 1,
      },

      /**
       * Ids for mirroring destination displays.
       */
      mirroringDestinationIds_: Array,
    };
  }

  displays: DisplayUnitInfo[];
  selectedDisplay?: DisplayUnitInfo;
  visualScale: number;
  private allowDisplayAlignmentApi_: boolean;
  private browserProxy_: DevicePageBrowserProxy;
  private hasDragStarted_: boolean;
  private invalidDisplayId_: string;
  private lastDragCoordinates_: {x: number, y: number}|null;
  private mirroringDestinationIds_: string[];
  private visualOffset_: {left: number, top: number};

  constructor() {
    super();

    this.visualOffset_ = {left: 0, top: 0};

    /**
     * Stores the previous coordinates of a display once dragging starts. Used
     * to calculate the delta during each step of the drag. Null when there is
     * no drag in progress.
     */
    this.lastDragCoordinates_ = null;

    this.browserProxy_ = DevicePageBrowserProxyImpl.getInstance();

    this.allowDisplayAlignmentApi_ =
        loadTimeData.getBoolean('allowDisplayAlignmentApi');

    this.invalidDisplayId_ = loadTimeData.getString('invalidDisplayId');

    this.hasDragStarted_ = false;

    this.mirroringDestinationIds_ = [];
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.initializeDrag(false);
  }

  /**
   * Called explicitly when |this.displays| and their associated |this.layouts|
   * have been fetched from chrome.
   */
  updateDisplays(
      displays: DisplayUnitInfo[], layouts: DisplayLayout[],
      mirroringDestinationIds: string[]): void {
    this.displays = displays;
    this.layouts = layouts;
    this.mirroringDestinationIds_ = mirroringDestinationIds;

    this.initializeDisplayLayout(displays, layouts);

    const self = this;
    const retry = 100;  // ms
    function tryCalcVisualScale(): void {
      if (!self.calculateVisualScale_()) {
        setTimeout(tryCalcVisualScale, retry);
      }
    }
    tryCalcVisualScale();

    // Enable keyboard dragging before initialization.
    this.keyboardDragEnabled = true;
    this.initializeDrag(
        !this.mirroring, this.$.displayArea,
        (id, amount) => this.onDrag_(id, amount));
  }

  /**
   * Calculates the visual offset and scale for the display area
   * (i.e. the ratio of the display area div size to the area required to
   * contain the DisplayUnitInfo bounding boxes).
   * @return Whether the calculation was successful.
   */
  private calculateVisualScale_(): boolean {
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
  }

  private getDivStyle_(
      id: string, _displayBounds: Bounds, _visualScale: number,
      offset?: number): string {
    // This matches the size of the box-shadow or border in CSS.
    const BORDER = 1;
    const MARGIN = 4;
    const OFFSET = offset || 0;
    const PADDING = 3;
    const bounds = this.getCalculatedDisplayBounds(id, /* notest */ true);
    if (!bounds) {
      return '';
    }
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
  }

  private getMirrorDivStyle_(
      mirroringDestinationIndex: number, mirroringDestinationDisplayNum: number,
      displays: DisplayUnitInfo[], visualScale: number): string {
    // All destination displays have the same bounds as the mirroring source
    // display, but we add a little offset to each destination display's bounds
    // so that they can be distinguished from each other in the layout.
    return this.getDivStyle_(
        displays[0].id, displays[0].bounds, visualScale,
        (mirroringDestinationDisplayNum - mirroringDestinationIndex) * -4);
  }

  private isSelected_(
      display: DisplayUnitInfo, selectedDisplay: DisplayUnitInfo): boolean {
    return display.id === selectedDisplay.id;
  }

  private dispatchSelectDisplayEvent_(displayId: DisplayUnitInfo['id']): void {
    const selectDisplayEvent =
        new CustomEvent('select-display', {composed: true, detail: displayId});
    this.dispatchEvent(selectDisplayEvent);
  }

  private onSelectDisplayClick_(e: DisplaySelectEvent): void {
    this.dispatchSelectDisplayEvent_(e.model.item.id);
    // Keep focused display in-sync with clicked display
    e.target.focus();
  }

  private onFocus_(e: DisplaySelectEvent): void {
    this.dispatchSelectDisplayEvent_(e.model.item.id);
    e.target.focus();
  }

  // Gets the display window position change announcement for a11y.
  private getPositionChangeAnnouncement_(deltaX: number, deltaY: number):
      string {
    let description = '';
    // Position was moved in both X and Y direction.
    if (deltaX !== 0 && deltaY !== 0) {
      if (deltaY > 0 && deltaX > 0) {
        description = 'displayPositionDownAndRight';
      } else if (deltaY > 0 && deltaX < 0) {
        description = 'displayPositionDownAndLeft';
      } else if (deltaY < 0 && deltaX > 0) {
        description = 'displayPositionUpAndRight';
      } else if (deltaY < 0 && deltaX < 0) {
        description = 'displayPositionUpAndLeft';
      }
    } else {
      // Position was moved in only one direction, either X or Y.
      if (deltaY > 0) {
        description = 'displayPositionDown';
      } else if (deltaY < 0) {
        description = 'displayPositionUp';
      } else if (deltaX > 0) {
        description = 'displayPositionRight';
      } else if (deltaX < 0) {
        description = 'displayPositionLeft';
      }
    }
    return this.i18n(description);
  }

  private onDrag_(id: string, amount: Position|null): void {
    id = id.substr(1);  // Skip prefix

    let newBounds: Bounds;
    if (!amount) {
      this.finishUpdateDisplayBounds(id);
      newBounds = this.getCalculatedDisplayBounds(id);
      this.lastDragCoordinates_ = null;
      // When the drag stops, remove the highlight around the display.
      this.browserProxy_.highlightDisplay(this.invalidDisplayId_);
    } else {
      this.browserProxy_.highlightDisplay(id);
      // Make sure the dragged display is also selected.
      if (id !== this.selectedDisplay!.id) {
        this.dispatchSelectDisplayEvent_(id);
      }

      const calculatedBounds = this.getCalculatedDisplayBounds(id);
      newBounds = {...calculatedBounds};
      newBounds.left += Math.round(amount.x / this.visualScale);
      newBounds.top += Math.round(amount.y / this.visualScale);

      if (this.displays.length >= 2) {
        newBounds = this.updateDisplayBounds(id, newBounds);
      }

      if (!this.lastDragCoordinates_) {
        this.hasDragStarted_ = true;
        this.lastDragCoordinates_ = {
          x: calculatedBounds.left,
          y: calculatedBounds.top,
        };
      }

      const deltaX = newBounds.left - this.lastDragCoordinates_.x;
      const deltaY = newBounds.top - this.lastDragCoordinates_.y;

      this.lastDragCoordinates_.x = newBounds.left;
      this.lastDragCoordinates_.y = newBounds.top;

      // Only call dragDisplayDelta() when there is a change in position.
      if (deltaX !== 0 || deltaY !== 0) {
        if (this.allowDisplayAlignmentApi_) {
          this.browserProxy_.dragDisplayDelta(
              id, Math.round(deltaX), Math.round(deltaY));
        }

        // Add ChromeVox announcement.
        const announcer = getAnnouncerInstance(this.$.displayArea);
        // Remove "role = alert" to avoid chromevox announcing "alert" before
        // message.
        strictQuery('#messages', announcer.shadowRoot, HTMLDivElement)
            .removeAttribute('role');
        // Announce the messages.
        announcer.announce(this.getPositionChangeAnnouncement_(deltaX, deltaY));
      }
    }

    const left =
        this.visualOffset_.left + Math.round(newBounds.left * this.visualScale);
    const top =
        this.visualOffset_.top + Math.round(newBounds.top * this.visualScale);
    const div = castExists(this.shadowRoot!.getElementById(`_${id}`));
    div.style.left = '' + left + 'px';
    div.style.top = '' + top + 'px';
    div.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'display-layout': DisplayLayoutElement;
  }
}

customElements.define(DisplayLayoutElement.is, DisplayLayoutElement);
