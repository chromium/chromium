// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cursor_tooltip.html.js';
import {toPixels} from './values_converter.js';

export interface CursorTooltipData {
  tooltipType: CursorTooltipType;
}

export enum CursorTooltipType {
  REGION_SEARCH = 0,
  TEXT_HIGHLIGHT = 1,
  CLICK_SEARCH = 2,
  LIVE_PAGE = 3,
}

export interface CursorTooltipElement {
  $: {
    cursorTooltip: HTMLElement,
  };
}

const CursorTooltipElementBase = I18nMixin(PolymerElement);

/*
 * Element responsible for showing the cursor tooltip.
 */
export class CursorTooltipElement extends CursorTooltipElementBase {
  static get is() {
    return 'cursor-tooltip';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      canShowTooltipFromPrefs: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('canShowTooltipFromPrefs'),
        reflectToAttribute: true,
      },

      tooltipMessage: {
        type: String,
        value: '',
        reflectToAttribute: true,
      },

      disableRendering: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      isPointerInside: Boolean,
    };
  }

  // The tooltip message string.
  private tooltipMessage: string;

  // The queued tooltip message string.
  private queuedTooltipMessage: string;

  // Whether or not the pointer is inside the web contents.
  private isPointerInside: boolean;

  // The queued tooltip offset pixels.
  private queuedOffsetLeftPx = 0;

  // The queued tooltip offset pixels.
  private queuedOffsetTopPx = 0;

  // Whether or not to disable rendering the tooltip.
  private disableRendering: boolean;

  // Whether or not to pause tooltip changes. If true, the tooltip changes
  // will be queued and applied when this becomes unset. This allows the
  // tooltip to know what to display as soon as the pointer is released
  // if the user is selecting something, without changing the tooltip
  // during the selection.
  private shouldPauseTooltipChanges = false;

  markPointerEnteredContentArea() {
    this.isPointerInside = true;
  }

  markPointerLeftContentArea() {
    this.isPointerInside = false;
  }

  hideTooltip() {
    this.disableRendering = true;
  }

  showTooltip() {
    this.disableRendering = false;
  }

  setPauseTooltipChanges(shouldPauseTooltipChanges: boolean) {
    this.shouldPauseTooltipChanges = shouldPauseTooltipChanges;
    if (!shouldPauseTooltipChanges) {
      this.setTooltipFromQueued();
    }
  }

  setTooltip(type: CursorTooltipType) {
    let offsetLeftPx = 0;
    let offsetTopPx = 0;
    if (type === CursorTooltipType.LIVE_PAGE) {
      offsetTopPx = 24;
      this.queuedTooltipMessage = this.i18n('cursorTooltipLivePageMessage');
    } else {
      // Add half the width of the cursor tooltip icon.
      offsetLeftPx += 16;
      // Add the height of the cursor tooltip icon, plus 8px.
      offsetTopPx += 40;
      // LINT.IfChange(CursorOffsetValues)
      if (type === CursorTooltipType.REGION_SEARCH) {
        offsetTopPx += 6;
        offsetLeftPx += 3;
        this.queuedTooltipMessage = this.i18n('cursorTooltipDragMessage');
      } else if (type === CursorTooltipType.TEXT_HIGHLIGHT) {
        offsetTopPx += 8;
        offsetLeftPx += 3;
        this.queuedTooltipMessage =
            this.i18n('cursorTooltipTextHighlightMessage');
      } else if (type === CursorTooltipType.CLICK_SEARCH) {
        offsetTopPx += 8;
        offsetLeftPx += 4;
        this.queuedTooltipMessage = this.i18n('cursorTooltipClickMessage');
      }
      // LINT.ThenChange(//chrome/browser/resources/lens/overlay/selection_overlay.ts:CursorOffsetValues)
    }
    this.queuedOffsetLeftPx = offsetLeftPx;
    this.queuedOffsetTopPx = offsetTopPx;
    if (!this.shouldPauseTooltipChanges) {
      this.setTooltipFromQueued();
    }
  }

  private setTooltipFromQueued() {
    this.style.setProperty('--offset-top', toPixels(this.queuedOffsetTopPx));
    this.style.setProperty('--offset-left', toPixels(this.queuedOffsetLeftPx));
    this.tooltipMessage = this.queuedTooltipMessage;
  }

  private getHiddenCursorClass(
      isPointerInside: boolean, canShowTooltipFromPrefs: boolean,
      disableRendering: boolean): string {
    return (isPointerInside && canShowTooltipFromPrefs && !disableRendering) ?
        '' :
        'hidden';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cursor-tooltip': CursorTooltipElement;
  }
}

customElements.define(CursorTooltipElement.is, CursorTooltipElement);
