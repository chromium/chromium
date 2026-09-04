// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';
import './favicon_group.js';
import './reopen_tabs.js';
import './sources_menu.js';
import './overflow_menu.js';
// <if expr="not is_android">
import '/shared/permission_dashboard.js';

import type {PermissionDashboardState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';
// </if>

// <if expr="is_android">
type PermissionDashboardState = any;
// </if>

import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextInfo} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import type {OverflowMenuElement} from './overflow_menu.js';
import type {SourcesMenuElement} from './sources_menu.js';
import {getCss} from './top_toolbar.css.js';
import {getHtml} from './top_toolbar.html.js';
import {recordAction} from './utils.js';

export interface TopToolbarElement {
  $: {
    closeButton: HTMLImageElement,
    overflowMenu: CrLazyRenderLitElement<OverflowMenuElement>,
    newThreadButton: HTMLImageElement,
    sourcesMenu: CrLazyRenderLitElement<SourcesMenuElement>,
    threadHistoryButton: HTMLImageElement,
  };
}

// <if expr="is_android">
const TopToolbarElementBase = CrLitElement;
// </if>
// <if expr="not is_android">
const TopToolbarElementBase = HelpBubbleMixinLit(CrLitElement);
// </if>

export class TopToolbarElement extends TopToolbarElementBase {
  static get is() {
    return 'top-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      contextInfos: {type: Array},
      darkMode: {
        type: Boolean,
        reflect: true,
        attribute: 'dark-mode',
      },
      isAiPage: {
        type: Boolean,
        reflect: true,
        attribute: 'is-ai-page',
      },
      enableOpenInNewTabButton: {
        type: Boolean,
        reflect: true,
      },
      title: {type: String},
      hideOverflowMenuButton_: {type: Boolean},
      showReopenTabs_: {type: Boolean},
      isExpandButtonEnabled: {type: Boolean},
      isPinButtonEnabled: {type: Boolean},
      isPinned: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      isAimEligible: {
        type: Boolean,
        reflect: true,
      },
      isCobrowseEligible: {type: Boolean},
      isHandshakeComplete: {type: Boolean},
      isUserSignedIn: {type: Boolean},
      onboardingTooltipShowing: {type: Boolean},
      lensSearchTooltipShowing: {type: Boolean},
      contextualTasksEnableSpatialModelToolbarLayout_: {type: Boolean},
      contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow_:
          {type: Boolean},
      overflowMenuOpen_: {type: Boolean},
      isSidePanelRearchitectureEnabled_: {
        type: Boolean,
        reflect: true,
        attribute: 'is-side-panel-rearchitecture-enabled',
      },
      webuiRoundedIconsEnabled_: {type: Boolean},
      permissionDashboardState: {type: Object},
    };
  }

  override accessor title: string = '';
  accessor contextInfos: ContextInfo[] = [];
  accessor darkMode: boolean = false;
  accessor isAiPage: boolean = loadTimeData.getBoolean('isAiPage');
  accessor isAimEligible: boolean = loadTimeData.getBoolean('isAimEligible');
  accessor isCobrowseEligible: boolean =
      loadTimeData.getBoolean('isCobrowseEligible');
  accessor isHandshakeComplete: boolean = false;
  accessor permissionDashboardState: PermissionDashboardState|null = null;
  protected accessor isSidePanelRearchitectureEnabled_: boolean =
      loadTimeData.getBoolean('contextualTasksSidePanelRearchitectureEnabled');
  accessor isUserSignedIn: boolean = true;
  accessor enableOpenInNewTabButton: boolean = false;
  accessor showReopenTabs_: boolean = false;
  accessor onboardingTooltipShowing: boolean = false;
  accessor lensSearchTooltipShowing: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  protected accessor isExpandButtonEnabled: boolean =
      loadTimeData.getBoolean('expandButtonEnabled');
  accessor isPinButtonEnabled: boolean =
      loadTimeData.getBoolean('enablePinButton');
  private hideOverflowMenuOnAiPageEnabled_: boolean =
      loadTimeData.getBoolean('hideMenuOnAiPageEnabled');
  protected accessor contextualTasksEnableSpatialModelToolbarLayout_: boolean =
      loadTimeData.getBoolean('contextualTasksEnableSpatialModelToolbarLayout');
  protected accessor contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow_:
          boolean = loadTimeData.getBoolean(
              'contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow');
  accessor hideOverflowMenuButton_: boolean =
      this.hideOverflowMenuOnAiPageEnabled_ && this.isAiPage;
  protected accessor isPinned: boolean =
      loadTimeData.getBoolean('isSidePanelPinned');
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
  protected accessor overflowMenuOpen_: boolean = false;
  protected accessor webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.onContextUpdated.addListener(contextInfos => {
        this.contextInfos = contextInfos;
      }),
      callbackRouter.setShowReopenTabs.addListener(show => {
        this.showReopenTabs_ = show;
      }),
      callbackRouter.onSidePanelPinStateChanged.addListener(
          (isPinned: boolean) => {
            this.isPinned = isPinned;
          }),
      callbackRouter.setExpandButtonEnabled.addListener((enabled: boolean) => {
        this.isExpandButtonEnabled = enabled;
      }),
      callbackRouter.onHandshakeComplete.addListener(() => {
        this.isHandshakeComplete = true;
      }),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  // <if expr="not is_android">
  override firstUpdated(_changedProperties: PropertyValues) {
    super.firstUpdated(_changedProperties);
    this.registerHelpBubble(
        'kContextualTasksWebUIToolbarElementId', '#top-row');
    this.registerHelpBubble(
        'kContextualTasksWebUIOverflowMenuElementId',
        '#overflowMenuButton');
    // Register help bubble only if 'G' logo is being shown.
    if ((this as unknown as HTMLElement)
            .shadowRoot?.querySelector('.top-toolbar-logo')) {
      this.registerHelpBubble(
          'kContextualTasksSuperGButtonElementId', '.top-toolbar-logo');
    }
  }
  // </if>

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('isAiPage') ||
        changedProperties.has('onboardingTooltipShowing') ||
        changedProperties.has('lensSearchTooltipShowing')) {
      this.hideOverflowMenuButton_ =
          this.isAiPage && this.hideOverflowMenuOnAiPageEnabled_;
      if (changedProperties.has('isAiPage') && !this.isAiPage) {
        this.isHandshakeComplete = false;
      }
      // <if expr="not is_android">
      if (this.isAiPage) {
        if (!this.onboardingTooltipShowing && !this.lensSearchTooltipShowing) {
          this.browserProxy_.handler.maybeTriggerPinningPromo();
        }
      }
      // </if>
    }
  }

  // If permission dashboard is not imported due to being on android, the
  // optional chaining (?) causes this to return false.
  protected isPermissionShowing_(): boolean {
    return this.isSidePanelRearchitectureEnabled_ &&
        (!!this.permissionDashboardState?.indicatorChip?.isVisible ||
         !!this.permissionDashboardState?.requestChip?.isVisible);
  }

  protected shouldShowSourcesMenuButton_(): boolean {
    return this.contextInfos.length > 0;
  }

  protected onPinClick_() {
    this.isPinned = !this.isPinned;
  }

  protected onCloseButtonClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.CloseSidePanel');
    this.browserProxy_.handler.closeSidePanel();
  }

  protected onNewThreadClick_() {
    this.fire('new-thread-click');
  }

  protected onThreadHistoryClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.OpenThreadHistory');
    this.browserProxy_.handler.showThreadHistory();
  }

  protected onOverflowMenuButtonClick_(e: Event) {
    recordAction('ContextualTasks.WebUI.UserAction.OpenOverflowMenu');
    this.$.overflowMenu.get().showAt(e.target as HTMLElement);
  }

  protected onOverflowMenuOpenChanged_(e: CustomEvent<{value: boolean}>) {
    this.overflowMenuOpen_ = e.detail.value;
  }

  protected onSourcesClick_(e: Event) {
    recordAction('ContextualTasks.WebUI.UserAction.OpenSourcesMenu');
    this.$.sourcesMenu.get().showAt(e.target as HTMLElement);
  }

  protected onOpenInNewTabClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.OpenInNewTab');
    this.browserProxy_.handler.moveTaskUiToNewTab();
  }

  protected onReopenTabsReopenClick_() {
    this.browserProxy_.handler.reopenTabs();
  }

  protected onReopenTabsDismissClick_() {
    this.showReopenTabs_ = false;
  }

  protected onLogoPointerdown_() {
    if (!this.isSidePanelRearchitectureEnabled_) {
      return;
    }
    this.browserProxy_.handler.onLogoPointerDown();
  }

  protected onLogoClick_(e: Event) {
    if (!this.isSidePanelRearchitectureEnabled_) {
      return;
    }
    // Keyboard synthetic clicks generate PointerEvents with an empty
    // pointerType in WebUI, whereas natural pointer clicks have a valid
    // pointerType (e.g., 'mouse', 'touch', 'pen').
    this.browserProxy_.handler.showPageInfoBubble(
        e instanceof PointerEvent && e.pointerType !== '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'top-toolbar': TopToolbarElement;
  }
}

customElements.define(TopToolbarElement.is, TopToolbarElement);
