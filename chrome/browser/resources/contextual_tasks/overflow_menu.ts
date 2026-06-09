// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './overflow_menu.css.js';
import {getHtml} from './overflow_menu.html.js';
import {recordAction} from './utils.js';

export interface OverflowMenuElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export class OverflowMenuElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-overflow-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableOpenInNewTabButton: {type: Boolean, reflect: true},
      isSmallDeviceFormFactor: {type: Boolean},
      isPinned: {type: Boolean},
      isPinButtonEnabled: {type: Boolean},
      isAiPage: {type: Boolean},
    };
  }

  accessor enableOpenInNewTabButton: boolean = false;
  accessor isSmallDeviceFormFactor: boolean =
      loadTimeData.getBoolean('isSmallDeviceFormFactor');
  accessor isPinned: boolean =
      loadTimeData.getBoolean('isSidePanelPinned');
  accessor isPinButtonEnabled: boolean =
      loadTimeData.getBoolean('enablePinButton');
  accessor isAiPage: boolean =
      loadTimeData.getBoolean('isAiPage');
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.onSidePanelPinStateChanged.addListener(
          (isPinned: boolean) => {
            this.isPinned = isPinned;
          }),
      callbackRouter.onAiPageStatusChanged.addListener(
          (isAiPage: boolean) => {
            this.isAiPage = isAiPage;
          }),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  showAt(target: HTMLElement) {
    this.$.menu.showAt(target, {
      noOffset: true,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  }

  close() {
    this.$.menu.close();
  }

  protected shouldShowPinButton_(): boolean {
    return this.isPinButtonEnabled && this.isAiPage;
  }

  protected getPinButtonTooltip_(): string {
    return this.isPinned ? loadTimeData.getString('unpinTooltip') :
                           loadTimeData.getString('pinTooltip');
  }

  protected onPinClick_() {
    this.close();
    this.isPinned = !this.isPinned;
    if (this.isPinned) {
      this.browserProxy_.handler.pinSidePanel();
    } else {
      this.browserProxy_.handler.unpinSidePanel();
    }
    this.fire('pin-click');
  }

  protected onThreadHistoryClick_() {
    this.close();
    recordAction('ContextualTasks.WebUI.UserAction.OpenThreadHistory');
    this.browserProxy_.handler.showThreadHistory();
  }

  protected onOpenInNewTabClick_() {
    this.close();
    recordAction('ContextualTasks.WebUI.UserAction.OpenInNewTab');
    this.browserProxy_.handler.moveTaskUiToNewTab();
  }

  protected onMyActivityClick_() {
    this.close();
    recordAction('ContextualTasks.WebUI.UserAction.OpenMyActivity');
    this.browserProxy_.handler.openMyActivityUi();
  }

  protected onFeedbackClick_() {
    this.close();
    recordAction('ContextualTasks.WebUI.UserAction.OpenHelp');
    this.browserProxy_.handler.openFeedbackUi();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-overflow-menu': OverflowMenuElement;
  }
}

customElements.define(OverflowMenuElement.is, OverflowMenuElement);
