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
  NONE = 0,
  REGION_SEARCH = 1,
  TEXT_HIGHLIGHT = 2,
  CLICK_SEARCH = 3,
  LIVE_PAGE = 4,
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
      canShowTooltipFromPrefs: Boolean,
      currentTooltip: Number,
      forceTooltipHidden: Boolean,
      isPointerInsideViewport: Boolean,
      tooltipMessage: String,
    };
  }

  // Whether the users has used the feature enough to not need the helping
  // tooltip anymore.
  private canShowTooltipFromPrefs: boolean =
      loadTimeData.getBoolean('canShowTooltipFromPrefs');

  // The current tooltip showing to the user.
  private currentTooltip: CursorTooltipType = CursorTooltipType.NONE;

  // Whether or not to force the tooltip as hidden.
  private forceTooltipHidden: boolean = false;

  // Whether or not the pointer is inside the web contents.
  private isPointerInsideViewport: boolean;

  // The tooltip message string.
  private tooltipMessage: string;

  // The queued tooltip type.
  private queuedTooltipType?: CursorTooltipType;

  // The queued tooltip message string.
  private queuedTooltipMessage: string;

  // The queued tooltip offset pixels.
  private queuedOffsetLeftPx = 0;

  // The queued tooltip offset pixels.
  private queuedOffsetTopPx = 0;

  // Whether or not to pause tooltip changes. If true, the tooltip changes
  // will be queued and applied when this becomes unset. This allows the
  // tooltip to know what to display as soon as the pointer is released
  // if the user is selecting something, without changing the tooltip
  // during the selection.
  private shouldPauseTooltipChanges = false;

  markPointerEnteredContentArea() {
    this.isPointerInsideViewport = true;
  }

  markPointerLeftContentArea() {
    this.isPointerInsideViewport = false;
  }

  hideTooltip() {
    this.forceTooltipHidden = true;
  }

  unhideTooltip() {
    this.forceTooltipHidden = false;
  }

  setPauseTooltipChanges(shouldPauseTooltipChanges: boolean) {
    this.shouldPauseTooltipChanges = shouldPauseTooltipChanges;
    if (!shouldPauseTooltipChanges && this.queuedTooltipType) {
      this.setTooltipImmediately(this.queuedTooltipType);
    }
  }

  setTooltip(type: CursorTooltipType) {
    if (this.shouldPauseTooltipChanges) {
      this.queuedTooltipType = type;
    } else {
      this.setTooltipImmediately(type);
    }
  }

  isTooltipVisible(): boolean {
    // Force hidden hides the cursor no matter what, so exit early.
    if (this.forceTooltipHidden) {
      return false;
    }

    // If the user is hovering over the live page, we want to show the tooltip
    // despite what the user prefs are set to.
    if (this.currentTooltip === CursorTooltipType.LIVE_PAGE &&
        this.isPointerInsideViewport) {
      return true;
    }

    // In all other cases, show the tooltip if the users prefs allows it, the
    // cursor is in the viewport, and the tooltip is set to a valid tooltip.
    return this.isPointerInsideViewport && this.canShowTooltipFromPrefs &&
        this.currentTooltip !== CursorTooltipType.NONE;
  }

  private setTooltipImmediately(tooltipType: CursorTooltipType) {
    this.currentTooltip = tooltipType;

    if (tooltipType === CursorTooltipType.NONE) {
      return;
    }
    this.queuedTooltipType = undefined;
    let offsetLeftPx = 0;
    let offsetTopPx = 0;
    let tooltipMessage = '';
    if (tooltipType === CursorTooltipType.LIVE_PAGE) {
      offsetTopPx = 24;
      tooltipMessage = this.i18n('cursorTooltipLivePageMessage');
    } else {
      // Add half the width of the cursor tooltip icon.
      offsetLeftPx += 16;
      // Add the height of the cursor tooltip icon, plus 8px.
      offsetTopPx += 40;
      // LINT.IfChange(CursorOffsetValues)
      if (tooltipType === CursorTooltipType.REGION_SEARCH) {
        offsetTopPx += 6;
        offsetLeftPx += 3;
        tooltipMessage = this.i18n('cursorTooltipDragMessage');
      } else if (tooltipType === CursorTooltipType.TEXT_HIGHLIGHT) {
        offsetTopPx += 8;
        offsetLeftPx += 3;
        tooltipMessage = this.i18n('cursorTooltipTextHighlightMessage');
      } else if (tooltipType === CursorTooltipType.CLICK_SEARCH) {
        offsetTopPx += 17;
        offsetLeftPx += 11;
        tooltipMessage = this.i18n('cursorTooltipClickMessage');
      }
      // LINT.ThenChange(//chrome/browser/resources/lens/overlay/selection_overlay.ts:CursorOffsetValues)
    }

    this.style.setProperty('--offset-top', toPixels(offsetTopPx));
    this.style.setProperty('--offset-left', toPixels(offsetLeftPx));
    this.tooltipMessage = tooltipMessage;
  }

  private getHiddenCursorClass(): string {
    return this.isTooltipVisible() ? '' : 'hidden';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cursor-tooltip': CursorTooltipElement;
  }
}

customElements.define(CursorTooltipElement.is, CursorTooltipElement);
