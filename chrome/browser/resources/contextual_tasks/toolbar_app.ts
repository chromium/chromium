// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './top_toolbar.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './toolbar_app.css.js';
import {getHtml} from './toolbar_app.html.js';
import {recordAction} from './utils.js';

export class ContextualTasksToolbarAppElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-toolbar-app';
  }

  static override get styles(): CSSResultGroup {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      threadTitle_: {type: String},
      darkMode_: {
        type: Boolean,
        reflect: true,
      },
      isAiPage_: {
        type: Boolean,
        reflect: true,
      },
      isAimEligible_: {
        type: Boolean,
        reflect: true,
      },
      isUserSignedIn_: {type: Boolean},
      onboardingTooltipShowing_: {type: Boolean},
      lensSearchTooltipShowing_: {type: Boolean},
    };
  }

  protected accessor threadTitle_: string = '';
  protected accessor darkMode_: boolean = loadTimeData.getBoolean('darkMode');
  protected accessor isAiPage_: boolean = loadTimeData.getBoolean('isAiPage');
  protected accessor isAimEligible_: boolean =
      loadTimeData.getBoolean('isAimEligible');
  protected accessor isUserSignedIn_: boolean =
      loadTimeData.getBoolean('isSignedIn');
  protected accessor onboardingTooltipShowing_: boolean = false;
  protected accessor lensSearchTooltipShowing_: boolean = false;

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.setThreadTitle.addListener((title: string) => {
        this.threadTitle_ = title;
        document.title = title || loadTimeData.getString('title');
      }),
      callbackRouter.onAiPageStatusChanged.addListener((isAiPage: boolean) => {
        this.isAiPage_ = isAiPage;
      }),
      callbackRouter.onSidePanelStateChanged.addListener(() => {
        // Handle theme update if side panel state changes
        const url = new URL(window.location.href);
        this.updateThemeFromUrl(url);
      }),
    ];

    const initialUrl = new URL(window.location.href);
    this.updateThemeFromUrl(initialUrl);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  private updateThemeFromUrl(url: URL) {
    const csParam = url.searchParams.get('cs');
    if (csParam === '0') {
      this.darkMode_ = false;
    } else if (csParam === '1') {
      this.darkMode_ = true;
    }
  }

  protected onNewThreadClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.OpenNewThread');
    this.browserProxy_.handler.createNewThread();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-toolbar-app': ContextualTasksToolbarAppElement;
  }
}

customElements.define(
    ContextualTasksToolbarAppElement.is, ContextualTasksToolbarAppElement);
