// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './extension.js';
import './toolbar_divider.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';

import {getCss} from './extensions.css.js';
import {getHtml} from './extensions.html.js';

// State pushed to Lit template for rendering.
export interface KeyedExtensionState {
  // Key so repeat directive can maintain consistent mapping between this
  // particular state and the Lit element.
  key: string;
  // Most of the state of the Lit element.
  state: ExtensionActionInfo;
  // Is this element sliding out (i.e. exiting)?
  // If true, this instance will be deleted from `keyedStates_` when the
  // slide-out animation completes by `onTransitionDone_()`.
  exiting?: boolean;
  // Should this element animate in (i.e. slide in)?
  animateIn?: boolean;
}

export class ExtensionsElement extends CrLitElement {
  static get is() {
    return 'webui-toolbar-extensions';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Array},
      keyedStates_: {type: Array},
    };
  }

  protected accessor state: ExtensionActionInfo[] = [];

  // Internal reactive state that includes exiting items.
  protected accessor keyedStates_: KeyedExtensionState[] = [];

  protected allExiting_(): boolean {
    return this.keyedStates_.length > 0 &&
        this.keyedStates_.every(s => s.exiting);
  }

  protected animateInDivider_(): boolean {
    return this.keyedStates_.length > 0 &&
        this.keyedStates_.every(s => s.animateIn);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('state')) {
      this.reconcileKeys_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    // Add listener to shadow root to catch bubbled transitionend and
    // transitioncancel events.
    this.shadowRoot.addEventListener(
        'transitionend', (e) => this.onTransitionDone_(e as TransitionEvent));
    this.shadowRoot.addEventListener(
        'transitioncancel',
        (e) => this.onTransitionDone_(e as TransitionEvent));
  }

  private reconcileKeys_() {
    const newMojoStates = this.state || [];
    const isInitial = this.keyedStates_.length === 0;

    // 1. Map new mojo states to KeyedExtensionState (all active).
    const newKeyedStates: KeyedExtensionState[] = newMojoStates.map(s => {
      const key = s.id;
      const animateIn =
          !isInitial && !this.keyedStates_.some(old => old.key === key);
      return {key, state: s, animateIn};
    });

    // 2. Find which keys were in the old `keyedStates_` but are not in
    // `newKeyedStates`. These are the ones that are "sliding-out".
    const newKeys = new Set(newKeyedStates.map(s => s.key));
    const missingOldStates = this.keyedStates_.filter(s => !newKeys.has(s.key));

    // 3. Re-insert "sliding-out" states (marked as exiting) into their old
    // positions in `keyedStates_` so they'll be rendered while "sliding-out"
    // if animations are enabled.
    const showAnimations = getComputedStyle(this)
                               .getPropertyValue('--animations-enabled')
                               .trim() !== '0';

    if (showAnimations) {
      // Sort missing states by their original index to preserve order during
      // insertion
      const oldKeyToIndex =
          new Map(this.keyedStates_.map((s, i) => [s.key, i]));
      missingOldStates.sort(
          (a, b) => oldKeyToIndex.get(a.key)! - oldKeyToIndex.get(b.key)!);

      // Insert them back with `exiting` set to true.
      for (const missing of missingOldStates) {
        const exitingState = {...missing, exiting: true};
        const originalIndex = oldKeyToIndex.get(missing.key)!;
        const insertIndex = Math.min(originalIndex, newKeyedStates.length);
        newKeyedStates.splice(insertIndex, 0, exitingState);
      }
    }

    this.keyedStates_ = newKeyedStates;
    this.updateVisibility_();

    // If the layout engine has already forced the exiting elements to 0 width
    // (preempting the transition), or if animations are disabled, remove them
    // immediately.
    this.updateComplete.then(() => {
      for (const el of this.shadowRoot.querySelectorAll('.exiting')) {
        const htmlEl = el as HTMLElement;
        // If it's already 0px wide, it won't transition.
        if (htmlEl.getBoundingClientRect().width === 0) {
          const key = htmlEl.dataset['key'];
          if (key) {
            this.keyedStates_ = this.keyedStates_.filter(s => s.key !== key);
          }
        }
      }
      this.updateVisibility_();
    });
  }

  // When an element finishes "sliding-out", remove it from `keyedStates_`.
  private onTransitionDone_(e: TransitionEvent) {
    // We only care about the width transition to trigger removal
    if (e.propertyName !== 'width') {
      return;
    }

    const target = e.target as HTMLElement;
    if (!target.classList.contains('exiting')) {
      return;
    }

    const key = target.dataset['key'];
    if (!key) {
      return;
    }

    // Remove the finished item (automatically triggers update)
    this.keyedStates_ = this.keyedStates_.filter(s => s.key !== key);
    this.updateVisibility_();
  }

  private updateVisibility_() {
    this.hidden = this.keyedStates_.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-toolbar-extensions': ExtensionsElement;
  }
}

customElements.define(ExtensionsElement.is, ExtensionsElement);
