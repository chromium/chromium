// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './top_toolbar.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ToolbarBrowserProxy} from './contextual_tasks_toolbar_browser_proxy.js';
import {ToolbarBrowserProxyImpl} from './contextual_tasks_toolbar_browser_proxy.js';
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

  private toolbarBrowserProxy_: ToolbarBrowserProxy =
      ToolbarBrowserProxyImpl.getInstance();
  private toolbarListenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    const toolbarCallbackRouter = this.toolbarBrowserProxy_.callbackRouter;
    this.toolbarListenerIds_ = [
      toolbarCallbackRouter.setThreadTitle.addListener((title: string) => {
        this.threadTitle_ = title;
        document.title = title || loadTimeData.getString('title');
      }),
      toolbarCallbackRouter.onSidePanelStateChanged.addListener(() => {
        // Handle theme update if side panel state changes
        const url = new URL(window.location.href);
        this.updateThemeFromUrl(url);
      }),
      toolbarCallbackRouter.onAiPageStatusChanged.addListener(
          (isAiPage: boolean) => {
            this.isAiPage_ = isAiPage;
          }),
    ];

    const initialUrl = new URL(window.location.href);
    this.updateThemeFromUrl(initialUrl);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.toolbarListenerIds_.forEach(
        id => this.toolbarBrowserProxy_.callbackRouter.removeListener(id));
    this.toolbarListenerIds_ = [];
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
    this.toolbarBrowserProxy_.handler.createNewThread();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-toolbar-app': ContextualTasksToolbarAppElement;
  }
}

customElements.define(
    ContextualTasksToolbarAppElement.is, ContextualTasksToolbarAppElement);
