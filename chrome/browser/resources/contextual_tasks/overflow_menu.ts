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
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
// <if expr="not is_android">
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
// </if>

import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './overflow_menu.css.js';
import {getHtml} from './overflow_menu.html.js';
import {hideUnboundedMenu, recordAction, showUnboundedMenu} from './utils.js';

export interface OverflowMenuElement {
  $: {menu: CrActionMenuElement};
}

// <if expr="is_android">
const OverflowMenuElementBase = CrLitElement;
// </if>
// <if expr="not is_android">
const OverflowMenuElementBase = HelpBubbleMixinLit(CrLitElement);
// </if>

export class OverflowMenuElement extends OverflowMenuElementBase {
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
      isUserFeedbackAllowed: {type: Boolean},
      contextualTasksEnableSpatialModelToolbarLayout: {type: Boolean},
      contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow:
          {type: Boolean},
      isAimEligible: {type: Boolean},
      isCobrowseEligible: {type: Boolean},
      isHandshakeComplete: {type: Boolean},
      webuiRoundedIconsEnabled_: {type: Boolean},
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
  accessor isUserFeedbackAllowed: boolean =
      loadTimeData.getBoolean('isUserFeedbackAllowed');
  accessor contextualTasksEnableSpatialModelToolbarLayout: boolean =
      loadTimeData.getBoolean('contextualTasksEnableSpatialModelToolbarLayout');
  accessor contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow:
      boolean = loadTimeData.getBoolean(
          'contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow');
  accessor isAimEligible: boolean = false;
  accessor isCobrowseEligible: boolean = false;
  accessor isHandshakeComplete: boolean = false;

  protected accessor webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];
// <if expr="not is_android">
  private helpBubbleRegistered_: boolean = false;
// </if>

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

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // <if expr="not is_android">
    const showPin = this.shouldShowPinButton_();
    if (showPin && !this.helpBubbleRegistered_) {
      this.registerHelpBubble(
          'kContextualTasksWebUIOverflowMenuPinButtonElementId', '#pinButton');
      this.helpBubbleRegistered_ = true;
    } else if (!showPin && this.helpBubbleRegistered_) {
      this.unregisterHelpBubble(
          'kContextualTasksWebUIOverflowMenuPinButtonElementId');
      this.helpBubbleRegistered_ = false;
    }
    // </if>
  }

  private get isUnboundedMenuEnabled_(): boolean {
    return loadTimeData.valueExists('contextualTasksUnboundedMenuEnabled') &&
        loadTimeData.getBoolean('contextualTasksUnboundedMenuEnabled');
  }

  showAt(target: HTMLElement) {
    this.$.menu.showAt(target, {
      noOffset: true,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      maxY: this.isUnboundedMenuEnabled_ ? Number.MAX_SAFE_INTEGER : undefined,
    });
    showUnboundedMenu(this.$.menu, this.isUnboundedMenuEnabled_, 'overflow');
  }

  close() {
    this.$.menu.close();
  }

  protected shouldShowPinButton_(): boolean {
    return this.isPinButtonEnabled && this.isAiPage &&
        this.isHandshakeComplete;
  }

  protected getPinButtonTooltip_(): string {
    return this.isPinned ? loadTimeData.getString('unpinTooltip') :
                           loadTimeData.getString('pinTooltip');
  }

  protected onPinClick_() {
    this.close();
    this.isPinned = !this.isPinned;
    if (this.isPinned) {
      recordAction('ContextualTasks.WebUI.UserAction.PinSidePanel');
      this.browserProxy_.handler.pinSidePanel();
    } else {
      recordAction('ContextualTasks.WebUI.UserAction.UnpinSidePanel');
      this.browserProxy_.handler.unpinSidePanel();
    }
    this.dispatchEvent(new CustomEvent('pin-click'));
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

  protected onHelpClick_() {
    this.close();
    recordAction('ContextualTasks.WebUI.UserAction.OpenHelp');
    this.browserProxy_.handler.openOverflowMenuHelpUi();
  }


  protected onFeedbackClick_() {
    this.close();
    recordAction('ContextualTasks.WebUI.UserAction.OpenFeedback');
    this.browserProxy_.handler.openFeedbackUi();
  }

  protected onNewThreadClick_() {
    this.close();
    this.fire('new-thread-click');
  }

  protected onOpenChanged_(e: CustomEvent<{value: boolean}>) {
    const menu = e.currentTarget as CrActionMenuElement;
    hideUnboundedMenu(
        menu, this.isUnboundedMenuEnabled_, e.detail.value, 'overflow');
    this.fire('open-changed', {value: e.detail.value});
  }

  protected shouldShowNewThreadInMenu_(): boolean {
    return this.isAimEligible &&
        this.contextualTasksEnableSpatialModelToolbarLayout &&
        this.contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow;
  }

  protected shouldShowThreadHistoryInMenu_(): boolean {
    return this.isSmallDeviceFormFactor ||
        (this.contextualTasksEnableSpatialModelToolbarLayout && this.isAiPage);
  }

  protected shouldShowOpenInNewTabInMenu_(): boolean {
    return !this.isSmallDeviceFormFactor &&
        !this.contextualTasksEnableSpatialModelToolbarLayout &&
        !this.contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow;
  }

  protected shouldShowMenuHeaderDivider_(): boolean {
    return this.shouldShowOpenInNewTabInMenu_() ||
        this.shouldShowThreadHistoryInMenu_() ||
        this.shouldShowPinButton_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-overflow-menu': OverflowMenuElement;
  }
}

customElements.define(OverflowMenuElement.is, OverflowMenuElement);
