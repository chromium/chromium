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
import './sources_menu.js';
import './overflow_menu.js';
// <if expr="not is_android">
import '/shared/permission_dashboard.js';

import type {PermissionChipDelegate} from '/shared/permission_chip_delegate.js';
import type {LhsChipIdentifier, PermissionDashboardState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {HelpBubbleMixinLit} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin_lit.js';

import type {InitialState} from './contextual_tasks_toolbar.mojom-webui.js';
// </if>

// <if expr="is_android">
type PermissionDashboardState = any;
type PermissionChipDelegate = any;
// </if>

import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextInfo} from './contextual_tasks_toolbar.mojom-webui.js';
import {ToolbarBrowserProxyImpl} from './contextual_tasks_toolbar_browser_proxy.js';
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
      isExpandButtonEnabled: {type: Boolean},
      isPinButtonEnabled: {type: Boolean},
      isPinned: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      isAimEligible: {
        type: Boolean,
        reflect: true,
      },
      isCobrowseEligible: {type: Boolean},
      isUserSignedIn: {type: Boolean},
      onboardingTooltipShowing: {type: Boolean},
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
      permissionChipDelegate_: {type: Object},
      profileAvatarUrl: {type: String},
    };
  }

  override accessor title: string = '';
  accessor contextInfos: ContextInfo[] = [];
  accessor darkMode: boolean = false;
  accessor isAiPage: boolean = loadTimeData.getBoolean('isAiPage');
  accessor isAimEligible: boolean = loadTimeData.getBoolean('isAimEligible');
  accessor isCobrowseEligible: boolean =
      loadTimeData.getBoolean('isCobrowseEligible');
  accessor permissionDashboardState: PermissionDashboardState|null = null;
  accessor profileAvatarUrl: string =
      loadTimeData.valueExists('profileAvatarUrl') ?
      loadTimeData.getString('profileAvatarUrl') :
      '';
  protected accessor isSidePanelRearchitectureEnabled_: boolean =
      loadTimeData.getBoolean('contextualTasksSidePanelRearchitectureEnabled');
  accessor isUserSignedIn: boolean = true;
  accessor enableOpenInNewTabButton: boolean = false;
  accessor onboardingTooltipShowing: boolean = false;
  protected accessor permissionChipDelegate_: PermissionChipDelegate|null =
      null;
  private toolbarBrowserProxy_: ToolbarBrowserProxyImpl =
      ToolbarBrowserProxyImpl.getInstance();
  private toolbarListenerIds_: number[] = [];
  // <if expr="not is_android">
  private sidePanelToolbarListenerIds_: number[] = [];
  // </if>
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
  private boundOnWindowBlur_: () => void = this.onWindowBlur_.bind(this);

  override connectedCallback() {
    super.connectedCallback();
    this.toolbarListenerIds_ = [
      this.toolbarBrowserProxy_.callbackRouter.onSidePanelPinStateChanged
          .addListener((isPinned: boolean) => {
            this.isPinned = isPinned;
          }),
      this.toolbarBrowserProxy_.callbackRouter.onContextUpdated.addListener(
          (contextInfos: ContextInfo[]) => {
            this.contextInfos = contextInfos;
          }),
      this.toolbarBrowserProxy_.callbackRouter.setExpandButtonEnabled
          .addListener((enabled: boolean) => {
            this.isExpandButtonEnabled = enabled;
          }),
      this.toolbarBrowserProxy_.callbackRouter.setProfileAvatarUrl.addListener(
          (avatarUrl: string) => {
            this.profileAvatarUrl = avatarUrl;
          }),
    ];
    window.addEventListener('blur', this.boundOnWindowBlur_);

    // <if expr="not is_android">
    const toolbarUiService = this.toolbarBrowserProxy_.toolbarUiService;
    const toolbarUiObserverCallbackRouter =
        this.toolbarBrowserProxy_.toolbarUiObserverCallbackRouter;
    this.sidePanelToolbarListenerIds_ = [
      toolbarUiObserverCallbackRouter.onPermissionDashboardStateChanged
          .addListener((state: PermissionDashboardState) => {
            this.permissionDashboardState = state;
          }),
    ];

    toolbarUiService.getInitialState().then((response: InitialState) => {
      if (!this.isConnected) {
        return;
      }
      if (response) {
        if (response.updateStream) {
          toolbarUiObserverCallbackRouter.$.bindHandle(
              response.updateStream.handle);
        }
        if (response.state !== undefined) {
          this.permissionDashboardState = response.state;
        }
      }
    }, () => {});

    this.permissionChipDelegate_ = {
      onChipClicked: (id: LhsChipIdentifier, isPointer: boolean) => {
        toolbarUiService.onChipClicked(id, isPointer);
      },
      onChipPointerEntered: (id: LhsChipIdentifier) => {
        toolbarUiService.onChipPointerEntered(id);
      },
      onChipPointerExited: (id: LhsChipIdentifier) => {
        toolbarUiService.onChipPointerExited(id);
      },
      onChipMousePressed: (id: LhsChipIdentifier, _isMiddleClick?: boolean) => {
        toolbarUiService.onChipMousePressed(id);
      },
      onChipExpandAnimationEnded: (id: LhsChipIdentifier) => {
        toolbarUiService.onChipExpandAnimationEnded(id);
      },
      onChipCollapseAnimationEnded: (id: LhsChipIdentifier) => {
        toolbarUiService.onChipCollapseAnimationEnded(id);
      },
    };
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.toolbarListenerIds_.forEach(
        id => this.toolbarBrowserProxy_.callbackRouter.removeListener(id));
    this.toolbarListenerIds_ = [];
    window.removeEventListener('blur', this.boundOnWindowBlur_);

    // <if expr="not is_android">
    const toolbarUiObserverCallbackRouter =
        this.toolbarBrowserProxy_.toolbarUiObserverCallbackRouter;
    this.sidePanelToolbarListenerIds_.forEach(
        id => toolbarUiObserverCallbackRouter.removeListener(id));
    this.sidePanelToolbarListenerIds_ = [];
    toolbarUiObserverCallbackRouter.$.close();
    // </if>
  }

  // Dismisses any open menu when the side panel loses focus. Clicks outside of
  // the side panel (e.g. on the page contents, the Lens crop frame, or a search
  // result in the sandboxed results frame) never reach this document, so
  // `cr-action-menu`'s own light dismiss does not run. See crbug.com/543760434.
  private onWindowBlur_() {
    this.$.overflowMenu.getIfExists()?.close();
    this.$.sourcesMenu.getIfExists()?.close();
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
        changedProperties.has('onboardingTooltipShowing')) {
      this.hideOverflowMenuButton_ =
          this.isAiPage && this.hideOverflowMenuOnAiPageEnabled_;
      // <if expr="not is_android">
      if (this.isAiPage) {
        if (!this.onboardingTooltipShowing) {
          this.toolbarBrowserProxy_.handler.maybeTriggerPinningPromo();
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
    this.toolbarBrowserProxy_.handler.closeSidePanel();
  }

  protected onNewThreadClick_() {
    this.fire('new-thread-click');
  }

  protected onThreadHistoryClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.OpenThreadHistory');
    this.toolbarBrowserProxy_.handler.showThreadHistory();
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
    this.toolbarBrowserProxy_.handler.moveTaskUiToNewTab();
  }


  protected onLogoPointerdown_() {
    if (!this.isSidePanelRearchitectureEnabled_) {
      return;
    }
    this.toolbarBrowserProxy_.handler.onLogoPointerDown();
  }

  protected onLogoClick_(e: Event) {
    if (!this.isSidePanelRearchitectureEnabled_) {
      return;
    }
    // Keyboard synthetic clicks generate PointerEvents with an empty
    // pointerType in WebUI, whereas natural pointer clicks have a valid
    // pointerType (e.g., 'mouse', 'touch', 'pen').
    this.toolbarBrowserProxy_.handler.showPageInfoBubble(
        e instanceof PointerEvent && e.pointerType !== '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'top-toolbar': TopToolbarElement;
  }
}

customElements.define(TopToolbarElement.is, TopToolbarElement);
