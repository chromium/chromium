// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_bar.js';
import './content_region.js';
import './extensions_bar.js';
import './icons.html.js';
import './side_panel.js';
import '/strings.m.js';
import './tab_strip.js';
import './webview.js';
import 'chrome://resources/cr_components/searchbox/searchbox.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert, assertNotReachedCase} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Tab} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import type {SearchboxElement} from 'chrome://resources/cr_components/searchbox/searchbox.js';
import {TrackedElementManager} from 'chrome://resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {FullscreenContext, PageHandlerFactory, SecurityIcon} from './browser.mojom-webui.js';
import {BrowserProxy} from './browser_proxy.js';
import type {ContentRegion} from './content_region.js';
import type {SidePanel} from './side_panel.js';
import {TabStrip} from './tab_strip.js';
import type {TabStripControllerDelegate} from './tab_strip_controller.js';
import {TabStripController} from './tab_strip_controller.js';

export interface WebuiBrowserAppElement {
  $: {
    address: SearchboxElement,
    appMenuButton: HTMLElement,
    avatarButton: HTMLElement,
    locationIconButton: HTMLElement,
    contentRegion: ContentRegion,
    sidePanel: SidePanel,
    tabstrip: TabStrip,
  };
}

export class WebuiBrowserAppElement extends CrLitElement implements
    TabStripControllerDelegate {
  static get is() {
    return 'webui-browser-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      backButtonDisabled_: {state: true, type: Boolean},
      forwardButtonDisabled_: {state: true, type: Boolean},
      fullscreenMode_:
          {type: String, reflect: true, attribute: 'fullscreen-mode'},
      showingSidePanel_: {state: true, type: Boolean},
      reloadOrStopIcon_: {state: true, type: String},
      showLocationIconButton_: {type: Boolean, reflect: true},
      locationIcon_: {state: true, type: String},
      tabStripInset_: {state: true, type: Number},
    };
  }

  private tabStripController_: TabStripController;
  private trackedElementManager_: TrackedElementManager;
  protected accessor backButtonDisabled_: boolean = true;
  protected accessor forwardButtonDisabled_: boolean = true;
  protected accessor fullscreenMode_: string = '';
  protected accessor reloadOrStopIcon_: string = 'icon-refresh';
  protected accessor showingSidePanel_: boolean = false;
  protected accessor showLocationIconButton_: boolean = false;
  protected accessor locationIcon_: string = 'NoEncryption';
  protected accessor tabStripInset_: number = 0;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();

    this.tabStripController_ =
        new TabStripController(this, this.$.tabstrip, this.$.contentRegion);
    this.trackedElementManager_ = TrackedElementManager.getInstance();

    const callbackRouter = BrowserProxy.getCallbackRouter();
    callbackRouter.showSidePanel.addListener(this.showSidePanel_.bind(this));
    callbackRouter.closeSidePanel.addListener(this.closeSidePanel_.bind(this));
    callbackRouter.onFullscreenModeChanged.addListener(
        this.onFullscreenModeChanged_.bind(this));
  }

  override async connectedCallback() {
    // Important. Properties are not reactive without calling
    // super.connectedCallback().
    super.connectedCallback();
    this.trackedElementManager_.startTracking(
        this.$.address, 'kLocationBarElementId');
    this.trackedElementManager_.startTracking(
        this.$.appMenuButton, 'kToolbarAppMenuButtonElementId');
    this.trackedElementManager_.startTracking(
        this.$.avatarButton, 'kToolbarAvatarButtonElementId');
    this.trackedElementManager_.startTracking(
        this.$.locationIconButton, 'kLocationIconElementId');
    this.trackedElementManager_.startTracking(
        this.$.contentRegion, 'kContentsContainerViewElementId');
    const {width} = await PageHandlerFactory.getRemote().getTabStripInset();
    this.tabStripInset_ = width;
  }

  // TabStripControllerDelegate:
  refreshLayout() {
    this.updateToolbarButtons_();
  }

  // Map from SecurityIcon values to the names of icons defined in
  // icons.html.ts.
  private securityIconToIconNameMap = new Map<SecurityIcon, string>([
    [SecurityIcon.HttpChromeRefresh, 'HttpChromeRefresh'],
    [SecurityIcon.SecurePageInfoChromeRefresh, 'SecurePageInfoChromeRefresh'],
    [SecurityIcon.NoEncryption, 'NoEncryption'],
    [
      SecurityIcon.NotSecureWarningChromeRefresh,
      'NotSecureWarningChromeRefresh',
    ],
    [SecurityIcon.BusinessChromeRefresh, 'BusinessChromeRefresh'],
    [SecurityIcon.DangerousChromeRefresh, 'DangerousChromeRefresh'],
    [SecurityIcon.ProductChromeRefresh, 'ProductChromeRefresh'],
    [SecurityIcon.ExtensionChromeRefresh, 'ExtensionChromeRefresh'],
    [SecurityIcon.OfflinePin, 'OfflinePin'],
  ]);

  async activeTabUpdated(tabData: Tab) {
    let displayUrl = '';
    const activeTabUrl = tabData.url.url;
    // TODO(webium): Should match
    // ChromeLocationBarModelDelegate::ShouldDisplayURL and
    // LocationBarModelImpl::GetFormattedURL logic.
    //
    // There are also likely some subtleties about what happens when the user
    // is typing and the tab navigates.
    const isNTP = activeTabUrl.startsWith('chrome://newtab');
    if (!isNTP) {
      displayUrl = activeTabUrl;

      if (this.$.contentRegion.activeWebview) {
        const securityIcon =
            await this.$.contentRegion.activeWebview.getSecurityIcon();
        const iconName = this.securityIconToIconNameMap.get(securityIcon);
        // Failure here indicates a new icon needs to be added to icons.html.ts
        // and then to |securityIconToIconNameMap|.
        assert(iconName);
        this.locationIcon_ = iconName;
      }
    }
    this.showLocationIconButton_ = !isNTP;
    this.$.address.setInputText(displayUrl);
    this.$.contentRegion.classList.toggle('modalScrim', tabData.isBlocked);
  }

  protected onLaunchDevtoolsClick_(_: Event) {
    BrowserProxy.getPageHandler().launchDevToolsForBrowser();
  }

  protected onAppMenuClick_(_: Event) {
    BrowserProxy.getPageHandler().openAppMenu();
  }

  protected onAvatarClick_(_: Event) {
    BrowserProxy.getPageHandler().openProfileMenu();
  }

  protected onMinimizeClick_(_: Event) {
    BrowserProxy.getPageHandler().minimize();
  }

  protected onMaximizeClick_(_: Event) {
    BrowserProxy.getPageHandler().maximize();
  }

  protected onRestoreClick_(_: Event) {
    BrowserProxy.getPageHandler().restore();
  }

  protected onCloseClick_(_: Event) {
    BrowserProxy.getPageHandler().close();
  }

  protected onBackClick_(_: Event) {
    if (this.$.contentRegion.activeWebview) {
      this.$.contentRegion.activeWebview.goBack();
    }
  }

  protected onForwardClick_(_: Event) {
    if (this.$.contentRegion.activeWebview) {
      this.$.contentRegion.activeWebview.goForward();
    }
  }

  protected onReloadOrStopClick_(_: Event) {
    if (this.$.contentRegion.activeWebview) {
      if (this.reloadOrStopIcon_ === 'icon-refresh') {
        this.$.contentRegion.activeWebview.reload();
      } else {
        this.$.contentRegion.activeWebview.stopLoading();
      }
    }
  }

  protected reloadOrStopTooltip_(): string {
    if (this.reloadOrStopIcon_ === 'icon-refresh') {
      return loadTimeData.getString('tooltipReload');
    } else {
      return loadTimeData.getString('tooltipStop');
    }
  }

  private async updateToolbarButtons_() {
    const webview = this.$.contentRegion.activeWebview;
    if (webview) {
      const [canGoBack, canGoForward] =
          await Promise.all([webview.canGoBack(), webview.canGoForward()]);
      this.backButtonDisabled_ = !canGoBack;
      this.forwardButtonDisabled_ = !canGoForward;
    } else {
      this.backButtonDisabled_ = true;
      this.forwardButtonDisabled_ = true;
    }
  }

  protected onTabClick_(e: CustomEvent) {
    this.tabStripController_.onTabClick(e);
  }

  protected onTabDragOutOfBounds_(e: CustomEvent) {
    this.tabStripController_.onTabDragOutOfBounds(e);
  }

  protected onTabClosed_(e: CustomEvent) {
    const tabId = e.detail.tabId;
    this.tabStripController_.removeTab(tabId);
  }

  protected onAddTabClick_(_: Event) {
    this.tabStripController_.addNewTab();
  }

  protected override firstUpdated() {
    BrowserProxy.getCallbackRouter().setFocusToLocationBar.addListener(
        this.setFocusToLocationBar.bind(this));
    BrowserProxy.getCallbackRouter().setReloadStopState.addListener(
        this.setReloadStopState.bind(this));
  }

  protected onTabDragMouseDown_(e: MouseEvent) {
    if (e.target instanceof TabStrip) {
      this.$.tabstrip.dragMouseDown(e);
      this.addEventListener('mouseup', this.onTabDragMouseUp_);
      this.addEventListener('mousemove', this.onTabDragMouseMove_);
    }
  }

  protected onTabDragMouseUp_(_: MouseEvent) {
    this.$.tabstrip.closeDragElement();
    this.removeEventListener('mouseup', this.onTabDragMouseUp_);
    this.removeEventListener('mousemove', this.onTabDragMouseMove_);
  }

  protected onTabDragMouseMove_(e: MouseEvent) {
    this.$.tabstrip.elementDrag(e);
  }

  protected setFocusToLocationBar(isUserInitiated: boolean) {
    this.$.address.focusInput();

    // If the user initiated the selection (e.g. by pressing Ctrl-L) we want to
    // select everything in order to make it easy to replace the URL. This is
    // also useful for some cases where we auto-focus (e.g. about:blank set as
    // the NTP) if they're not actively using the omnibox, which we check by
    // looking at the focus. See OmniBoxViewViews::SetFocus() for the
    // inspiration.
    if (isUserInitiated || this.shadowRoot.activeElement !== this.$.address) {
      this.$.address.selectAll();
    }
  }

  protected setReloadStopState(isLoading: boolean) {
    this.reloadOrStopIcon_ = isLoading ? 'icon-clear' : 'icon-refresh';
  }


  protected showSidePanel_(guestContentsId: number, title: string) {
    this.showingSidePanel_ = true;
    this.$.sidePanel.show(guestContentsId, title);
  }

  protected closeSidePanel_() {
    this.$.sidePanel.close();
    this.showingSidePanel_ = false;
  }

  // This function is called when the side panel closes itself. For example,
  // when user clicks the close "x" button.
  protected onSidePanelClosed_() {
    this.showingSidePanel_ = false;
  }

  protected onFullscreenModeChanged_(
      isFullscreen: boolean, context?: FullscreenContext) {
    if (!isFullscreen) {
      this.fullscreenMode_ = '';
    } else {
      // When fullscreen is true, we should always have a context
      assert(
          context !== undefined,
          'Context must be provided when isFullscreen is true');

      switch (context) {
        case FullscreenContext.kTab:
          this.fullscreenMode_ = 'tab';
          break;
        case FullscreenContext.kBrowser:
          this.fullscreenMode_ = 'browser';
          break;
        default:
          assertNotReachedCase(context);
      }
    }
  }

  protected onLocationIconClick_(_: Event) {
    if (this.$.contentRegion.activeWebview) {
      this.$.contentRegion.activeWebview.openPageInfoMenu();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-app': WebuiBrowserAppElement;
  }
}

customElements.define(WebuiBrowserAppElement.is, WebuiBrowserAppElement);
