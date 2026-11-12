// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import './icons.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {BUTTON_LEFT, HelpBubbleAnchorMixin, HighlightTracker} from '/shared/toolbar_button.js';
import type {GlicActorTaskIconState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {CollapsibleLabelButtonMixin} from './collapsible_label_button.js';
import {getCss} from './glic_actor_task_icon.css.js';
import {getHtml} from './glic_actor_task_icon.html.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export interface GlicActorTaskIconElement {
  $: {
    button: ToolbarChipButtonElement,
  };
}

const GlicActorTaskIconElementBase =
    HelpBubbleAnchorMixin(CollapsibleLabelButtonMixin(CrLitElement));

/**
 * WebUI counterpart of the native ToolbarGlicActorTaskIcon. While a Glic actor
 * task is active, it is shown as the trailing half of the Glic split button
 * (flat start edge, next to `<glic-button>`). It renders a cursor-spark icon,
 * optionally followed by a nudge label (e.g. "Check task") when a task needs
 * attention. The label collapses when toolbar space is constrained. Clicking it
 * toggles the task list bubble; `state.isPressed` reflects whether that bubble
 * is open. All state is driven by WebUIGlicActorTaskIconControl in the browser.
 */
export class GlicActorTaskIconElement extends GlicActorTaskIconElementBase {
  static get is() {
    return 'glic-actor-task-icon';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...super.properties,
      state: {type: Object},
    };
  }

  accessor state: GlicActorTaskIconState = {
    shouldShow: false,
    nudgeLabel: null,
    tooltip: '',
    isPressed: false,
  };

  highlightTracker: HighlightTracker = new HighlightTracker();
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const oldState = changedProperties.get('state');
    if (changedProperties.has('state') &&
        (!oldState || oldState.shouldShow !== this.state.shouldShow ||
         oldState.nudgeLabel !== this.state.nudgeLabel)) {
      this.fire('request-layout');
    }
  }

  override focus() {
    this.$.button.focus();
  }

  override shouldBeShown(): boolean {
    return this.state.shouldShow;
  }

  protected override hasLabel_(): boolean {
    return !!this.getLabel_();
  }

  protected getLabel_(): string {
    return this.state.nudgeLabel || '';
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.state.tooltip);
  }

  protected onPointerdown_(e: PointerEvent) {
    this.highlightTracker.onPointerdown(e);
    // Matches Views' MenuButtonController: a left mouse press activates
    // immediately; touch activates on tap (click).
    if (e.pointerType !== 'mouse' || e.button !== BUTTON_LEFT) {
      return;
    }
    this.activate_(e);
  }

  protected onClick_(e: PointerEvent) {
    // Keyboard (detail === 0) and touch clicks activate here. Mouse clicks
    // were already handled on pointerdown.
    if (e.detail === 0 || (e.pointerType && e.pointerType !== 'mouse')) {
      this.activate_(e);
    }
  }

  protected onKeydown_(e: KeyboardEvent) {
    // Matches Views' MenuButtonController::OnKeyPressed: Up and Down activate
    // on keydown. Enter and Space synthesize a click on the inner <button>.
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      this.activate_(e);
    }
  }

  private activate_(e: Event) {
    if (e instanceof PointerEvent && this.highlightTracker.shouldSkipClick(e)) {
      return;
    }
    TrackedElementManager.getInstance().notifyElementActivated(this);
    this.browserProxy_.toolbarUIHandler.onGlicActorTaskIconClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-actor-task-icon': GlicActorTaskIconElement;
  }
}

customElements.define(GlicActorTaskIconElement.is, GlicActorTaskIconElement);
