// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';

import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';

export interface ContextualTasksComposeboxElement {
  $: {
    composebox: ComposeboxElement,
  };
}

export class ContextualTasksComposeboxElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-composebox';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      composeboxHeight_: {type: Number},
      composeboxDropdownHeight_: {type: Number},
      isComposeboxFocused_: {type: Boolean, reflect: true},
      showContextMenu_: {
        type: Boolean,
        value: loadTimeData.getBoolean('composeboxShowContextMenu'),
      },
    };
  }

  protected accessor composeboxHeight_: number = 0;
  protected accessor composeboxDropdownHeight_: number = 0;
  protected accessor isComposeboxFocused_: boolean = false;
  protected accessor showContextMenu_: boolean =
      loadTimeData.getBoolean('composeboxShowContextMenu');
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();

    const composebox = this.$.composebox;
    if (composebox) {
      this.eventTracker_.add(composebox, 'composebox-focus-in', () => {
        this.isComposeboxFocused_ = true;
      });
      this.eventTracker_.add(composebox, 'composebox-focus-out', () => {
        this.isComposeboxFocused_ = false;
        if (composebox.animationState === GlowAnimationState.SUBMITTING ||
            composebox.animationState === GlowAnimationState.LISTENING) {
          return;
        }
        composebox.animationState = GlowAnimationState.NONE;
      });
      this.eventTracker_.add(composebox, 'composebox-submit', () => {
        // Clear the composebox text after submitting.
        composebox.clearInput();
        composebox.clearAutocompleteMatches();
      });
      this.eventTracker_.add(
          composebox, 'composebox-resize', (e: CustomEvent) => {
            if (e.detail.carouselHeight !== undefined) {
              composebox.style.setProperty(
                  '--carousel-height', `${e.detail.carouselHeight}px`);
            }
          });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-composebox': ContextualTasksComposeboxElement;
  }
}

customElements.define(
    ContextualTasksComposeboxElement.is, ContextualTasksComposeboxElement);
