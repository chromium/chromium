// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {CueLog} from './browser_proxy.js';
import {BrowserProxy} from './browser_proxy.js';

export class ContextualCueingInternalsAppElement extends CrLitElement {
  static get is() {
    return 'contextual-cueing-internals-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      shownCues_: {type: Array},
    };
  }

  protected accessor shownCues_: CueLog[] = [];
  private browserProxy_ = BrowserProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.loadShownCues_();
  }

  private async loadShownCues_() {
    try {
      const {cues} = await this.browserProxy_.handler.getShownCues();
      this.shownCues_ = cues.reverse();
    } catch (e) {
      console.error('Error fetching shown cues:', e);
    }
  }

  protected onFeedbackClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const index = Number(target.dataset['index']);
    const cue = this.shownCues_[index];
    if (!cue) {
      return;
    }
    const baseUrl =
        'https://docs.google.com/forms/d/e/1FAIpQLSecbl33hK_e5SKzehIJPApt5y0zs9lSRnxtO6AK5p3y76ffEw/viewform';
    const params = new URLSearchParams({
      'usp': 'pp_url',
      'entry.679555410': cue.anchoredMessageText || '',
      'entry.1079969148': cue.url || '',
      'entry.901029402': cue.prompt || '',
    });
    window.open(`${baseUrl}?${params.toString()}`, '_blank');
  }
}

customElements.define(
    ContextualCueingInternalsAppElement.is,
    ContextualCueingInternalsAppElement);

declare global {
  interface HTMLElementTagNameMap {
    'contextual-cueing-internals-app': ContextualCueingInternalsAppElement;
  }
}
