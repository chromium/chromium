// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/icon_from_table.js';
import './toolbar_chip_button.js';

import {skColorToRgba} from '//resources/js/color_utils.js';
import {ensureTransitionEndEvent} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {MenuSourceType} from '//resources/mojo/ui/base/mojom/menu_source_type.mojom-webui.js';
import {AnimationTracker} from '/shared/animation_tracker.js';
import type {PageActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {PageActionId, PageActionTrigger} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './page_action_icon.css.js';
import {getHtml} from './page_action_icon.html.js';
import {getClickSourceType} from './toolbar_button.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export interface PageActionIconElement {
  $: {
    button: ToolbarChipButtonElement,
  };
}

export class PageActionIconElement extends CrLitElement {
  static get is() {
    return 'page-action-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Object},

      // Draw a focus ring as if focused (e.g., the AI mode chip when the user
      // tabs through the Omnibox suggestions, keeping real focus in the text
      // field).
      forceFocusRing: {type: Boolean},

      // Some additional style = attributes for the chip element.
      chipStyleOverride_: {type: String, state: true},
    };
  }

  accessor state: PageActionState = {
    pageActionId: PageActionId.kActionAiMode,
    accessibleName: '',
    tooltipText: '',
    icon: {handleId: 0n},
    text: '',
    shouldShowChip: false,
    shouldAnimateChipIn: false,
    shouldAnimateChipOut: false,
    backgroundColorOverride: null,
  };

  accessor forceFocusRing: boolean = false;

  protected accessor chipStyleOverride_: string|null = null;

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private wasShowingChip_: boolean = false;

  override focus() {
    this.$.button.focus();
  }

  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);
    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      this.wasShowingChip_ =
          oldState?.pageActionId === this.state.pageActionId &&
          (oldState?.shouldShowChip ?? false);
      if (this.state.backgroundColorOverride) {
        this.chipStyleOverride_ = `--toolbar-chip-bg-color: ${
            skColorToRgba(this.state.backgroundColorOverride)};`;
      } else {
        this.chipStyleOverride_ = null;
      }
    }
  }

  protected shouldShowLabel_(): boolean {
    return this.state.shouldShowChip && !!this.state.text;
  }

  protected shouldAnimate_(): boolean {
    if (!this.state.text) {
      return false;
    }
    return this.state.shouldShowChip ?
        this.state.shouldAnimateChipIn :
        (this.wasShowingChip_ && this.state.shouldAnimateChipOut);
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('forceFocusRing')) {
      this.$.button.toggleAttribute('force-focus-ring', this.forceFocusRing);
    }
    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      if (!oldState || oldState.shouldShowChip !== this.state.shouldShowChip) {
        const fireIpc = () => {
          this.browserProxy_.toolbarUIHandler.onPageActionChipShowingChanged(
              this.state.pageActionId);
        };

        if (!AnimationTracker.showAnimations) {
          fireIpc();
          return;
        }

        const button = this.$.button;
        button.addEventListener('transitionend', fireIpc, {once: true});
        ensureTransitionEndEvent(button);
      }
    }
  }

  protected getAriaLabel_(): string {
    return this.state.accessibleName;
  }

  protected onClick_(e: Event) {
    this.browserProxy_.toolbarUIHandler.onPageActionClick(
        this.state.pageActionId, this.getPageActionTrigger_(e));
  }

  private getPageActionTrigger_(e: Event): PageActionTrigger {
    const sourceType = getClickSourceType(e);
    switch (sourceType) {
      case MenuSourceType.kTouch:
        return PageActionTrigger.kGesture;
      case MenuSourceType.kKeyboard:
        return PageActionTrigger.kKeyboard;
      case MenuSourceType.kMouse:
        return PageActionTrigger.kMouse;
      default:
        throw new Error('Unknown sourceType ' + sourceType);
    }
  }

  protected onPointerdown_() {
    this.browserProxy_.toolbarUIHandler.onPageActionPointerDown(
        this.state.pageActionId);
  }

  protected onPointerenter_() {
    this.fire('chip-pointerenter');
  }

  protected onPointerleave_() {
    this.fire('chip-pointerleave');
  }

  protected onPointercancel_() {
    this.fire('chip-pointercancel');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'page-action-icon': PageActionIconElement;
  }
}

customElements.define(PageActionIconElement.is, PageActionIconElement);
