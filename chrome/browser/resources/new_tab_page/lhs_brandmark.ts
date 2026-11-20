// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {recordThreadsRailAction, ThreadsRailAction} from 'chrome://resources/cr_components/composebox/threads_rail.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {I18nMixinLit, loadTimeData} from './i18n_setup.js';
import {getCss} from './lhs_brandmark.css.js';
import {getHtml} from './lhs_brandmark.html.js';
import {recordThreadsRailExpandedAction, ThreadsRailExpandedAction} from './metrics_utils.js';
import {WindowProxy} from './window_proxy.js';

/**
 * @fileoverview Persistent Google brandmark overlay for <cr-left-hand-side>
 * that transitions between the folded 26px Super G button and the expanded
 * 92x30 Google wordmark across the rail's expand/collapse cross-fade. The
 * wordmark markup, styles and motion timings are copied from Search. Keep in
 * sync. Mapping doc: go/ntp-lhs-brandmark.
 */

const LhsBrandmarkElementBase = I18nMixinLit(CrLitElement);

export interface LhsBrandmarkElement {
  $: {
    logoButton: CrIconButtonElement,
    wordmark: HTMLAnchorElement,
  };
}

export class LhsBrandmarkElement extends LhsBrandmarkElementBase {
  static get is() {
    return 'ntp-lhs-brandmark';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      expanded: {
        type: Boolean,
        reflect: true,
      },
      googleBaseUrl_: {type: String},
      isDark: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  // Mirrors Search's data-lhsx attribute on .lhs-brandmark.
  accessor expanded: boolean = false;
  // Selects the dark-mode wordmark fill in lhs_brandmark.css.
  accessor isDark: boolean = false;
  private accessor googleBaseUrl_: string =
      loadTimeData.valueExists('googleBaseUrl') ?
      loadTimeData.getString('googleBaseUrl') :
      '';

  protected getGoogleBaseUrl_(): string {
    return this.parseHttpUrl_(this.googleBaseUrl_) ? this.googleBaseUrl_ : '';
  }

  // NTP: stopPropagation prevents folded logo clicks from bubbling up to expand
  // the rail; modified <a> clicks fall through to native new-tab/window
  // navigation.
  protected onLogoClick_(e: MouseEvent) {
    if (e.button !== 0) {
      return;
    }
    e.stopPropagation();
    const googleBaseUrl = this.getGoogleBaseUrl_();
    if (!googleBaseUrl) {
      e.preventDefault();
      return;
    }
    if (this.expanded) {
      recordThreadsRailExpandedAction(ThreadsRailExpandedAction.LOGO_CLICKED);
    } else {
      recordThreadsRailAction(ThreadsRailAction.LOGO_CLICKED);
    }
    // Let native <a> handling open modified clicks (e.g. Ctrl/Cmd+Click for a
    // new tab) without overriding them via WindowProxy.navigate().
    if (e.currentTarget instanceof HTMLAnchorElement &&
        (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey)) {
      return;
    }
    e.preventDefault();
    WindowProxy.getInstance().navigate(googleBaseUrl);
  }

  private parseHttpUrl_(rawUrl: string): URL|null {
    if (!rawUrl) {
      return null;
    }
    try {
      const url = new URL(rawUrl);
      return url.protocol === 'https:' || url.protocol === 'http:' ? url : null;
    } catch {
      return null;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-lhs-brandmark': LhsBrandmarkElement;
  }
}

customElements.define(LhsBrandmarkElement.is, LhsBrandmarkElement);
