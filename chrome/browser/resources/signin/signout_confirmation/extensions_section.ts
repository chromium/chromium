// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '/strings.m.js';

import type {CrCollapseElement} from '//resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrExpandButtonElement} from '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './extensions_section.css.js';
import {getHtml} from './extensions_section.html.js';
import type {ExtensionInfo} from './signout_confirmation.mojom-webui.js';

// Update request count, to be used along the transition duration to compute the
// interval time requests.
const UPDATE_REQUEST_COUNT: number = 10;

export interface ExtensionsSectionElement {
  $: {
    checkbox: CrCheckboxElement,
    expandButton: CrExpandButtonElement,
    collapse: CrCollapseElement,
  };
}

export class ExtensionsSectionElement extends CrLitElement {
  static get is() {
    return 'extensions-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      accountExtensions: {type: Array},
      title_: {type: String},
      tooltip_: {type: String},
      expanded_: {type: Boolean},
    };
  }

  accessor accountExtensions: ExtensionInfo[] = [];
  protected accessor title_: string = '';
  protected accessor tooltip_: string = '';
  protected accessor expanded_: boolean = false;

  // Animation variables used to update the main view height based on the
  // collapse animation duration. Initialized to 0 and gets their values in
  // `firstUpdated()` which are not expected to be modified later.
  private updateHeightInterval_: number = 0;
  private collapseAnimationDuration_: number = 0;

  override firstUpdated() {
    // Compute the animation duration/intervals once on startup.
    this.collapseAnimationDuration_ =
        parseInt(getComputedStyle(this).getPropertyValue(
            '--iron-collapse-transition-duration'));
    this.updateHeightInterval_ =
        this.collapseAnimationDuration_ / UPDATE_REQUEST_COUNT;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('accountExtensions')) {
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'extensionsSectionTitle', this.accountExtensions.length)
          .then(title => this.onTitleSet_(title));
      PluralStringProxyImpl.getInstance()
          .getPluralString(
              'extensionsSectionTooltip', this.accountExtensions.length)
          .then(tooltip => this.tooltip_ = tooltip);
    }
  }

  checked(): boolean {
    return this.$.checkbox.checked;
  }

  protected onExpandChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;

    // Stagger the update by `this.updateHeightInterval_` so it will fire its
    // last update-view-height after the animation has finished.
    setTimeout(() => {
      this.updateViewHeightInterval_(this.updateHeightInterval_);
    }, this.updateHeightInterval_);
  }

  private async onTitleSet_(title: string) {
    this.title_ = title;
    await this.updateComplete;
    this.fire('update-view-height');
  }

  // Fire repetitive updates to the parent view height separated by the computed
  // interval, until the animation duration elapsed.
  private updateViewHeightInterval_(timeElapsed: number) {
    this.fire('update-view-height');
    // Animation time elapsed, animation should match the collapse animation. No
    // more view updates needed.
    if (timeElapsed >= this.collapseAnimationDuration_) {
      return;
    }

    // Trigger next update interval with the updated elapsed time.
    setTimeout(() => {
      this.updateViewHeightInterval_(timeElapsed + this.updateHeightInterval_);
    }, this.updateHeightInterval_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-section': ExtensionsSectionElement;
  }
}

customElements.define(ExtensionsSectionElement.is, ExtensionsSectionElement);
