// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_lens_search.js';
import '//resources/cr_components/composebox/current_tab_chip.js';
import './omnibox_contextual_entrypoint_button.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {OmniboxContextualEntrypointButtonElement} from './omnibox_contextual_entrypoint_button.js';
import {browserProxyFactory} from './omnibox_popup.mojom-webui.js';
import type {BrowserProxy} from './omnibox_popup.mojom-webui.js';
import {getCss} from './omnibox_popup_contextual_entrypoint.css.js';
import {getHtml} from './omnibox_popup_contextual_entrypoint.html.js';

export class OmniboxPopupContextualEntrypointElement extends CrLitElement {
  static get is() {
    return 'omnibox-popup-contextual-entrypoint';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      inputState: {type: Object},
      isLensSearchEligible: {type: Boolean},
      isLensIconEligible: {type: Boolean},
      isContentSharingEnabled: {type: Boolean},
      searchboxLayoutMode: {
        type: String,
        reflect: true,
      },
      isLensChipShown_: {type: Boolean},
      isLensIconShown_: {type: Boolean},
      currentTabForChip_: {type: Object},
      isCurrentTabChipShown_: {type: Boolean},
      applyContextButtonBackground_: {type: Boolean},
      usePecApi_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      showContextButtonSuggestionLabel_: {type: Boolean},
    };
  }

  accessor inputState: InputState|null = null;
  accessor isLensSearchEligible: boolean = false;
  accessor isLensIconEligible: boolean = false;
  accessor isContentSharingEnabled: boolean = false;
  accessor searchboxLayoutMode: string = '';

  protected accessor isLensChipShown_: boolean = false;
  protected accessor isLensIconShown_: boolean = false;
  protected accessor currentTabForChip_: TabInfo|null = null;
  protected accessor isCurrentTabChipShown_: boolean = false;
  protected accessor applyContextButtonBackground_: boolean = false;
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor showContextButtonSuggestionLabel_: boolean =
      loadTimeData.getBoolean('omniboxShowContextButtonSuggestionLabel');

  private isCurrentTabChipEnabled_: boolean =
      loadTimeData.getBoolean('composeboxShowCurrentTabChip');
  private contextButtonHasBackground_: boolean =
      loadTimeData.getBoolean('contextButtonHasBackground');

  private browserProxy_: BrowserProxy;
  private searchboxBrowserProxy_: SearchboxBrowserProxy;
  private popupListenerIds_: number[] = [];

  constructor() {
    super();
    this.searchboxBrowserProxy_ = SearchboxBrowserProxy.getInstance();
    this.browserProxy_ = browserProxyFactory.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.popupListenerIds_ = [
      this.browserProxy_.callbackRouter.onShow.addListener(
          this.onShow_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    for (const listenerId of this.popupListenerIds_) {
      this.browserProxy_.callbackRouter.removeListener(listenerId);
    }
    this.popupListenerIds_ = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedProps = changedProperties as Map<PropertyKey, unknown>;

    if (changedProps.has('isContentSharingEnabled') ||
        changedProps.has('isLensSearchEligible') ||
        changedProps.has('isLensIconEligible') ||
        changedProps.has('currentTabForChip_') ||
        changedProps.has('inputState')) {
      this.isCurrentTabChipShown_ = this.isContentSharingEnabled &&
          this.isLensSearchEligible && this.computeShowCurrentTabChip_();
      this.isLensIconShown_ =
          this.isContentSharingEnabled && this.isLensIconEligible;
      this.isLensChipShown_ = this.isContentSharingEnabled &&
          this.isLensSearchEligible && !this.isCurrentTabChipShown_;
      this.applyContextButtonBackground_ =
          this.contextButtonHasBackground_ && !this.isLensChipShown_;
    }
  }

  getContextEntrypointElement(): OmniboxContextualEntrypointButtonElement|null {
    return this.shadowRoot
               ?.querySelector<OmniboxContextualEntrypointButtonElement>(
                   '#context') ??
        null;
  }

  protected shouldHideEntrypointButton_(): boolean {
    return this.searchboxLayoutMode === 'Compact';
  }

  protected onLensSearchClick_() {
    this.searchboxBrowserProxy_.handler.openLensSearch();
  }

  protected async refreshCurrentTabForChip_() {
    // TODO (b/537859769) - Replace getRecentTabs with a dedicated handler for
    // current tab chip.
    const {tabs} = await this.searchboxBrowserProxy_.handler.getRecentTabs();
    this.currentTabForChip_ =
        tabs.find(tab => tab.showInCurrentTabChip) || null;
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
  }>) {
    this.searchboxBrowserProxy_.handler.addTabContext(
        e.detail.id, /*delayUpload=*/ false);
  }

  protected computeShowCurrentTabChip_() {
    const browserTabsAllowedByPecApi = !this.usePecApi_ ||
        (!!this.inputState &&
         this.inputState.allowedInputTypes.includes(InputType.kBrowserTab));
    return this.isCurrentTabChipEnabled_ && !!this.currentTabForChip_ &&
        browserTabsAllowedByPecApi;
  }

  private onShow_() {
    this.refreshCurrentTabForChip_();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-contextual-entrypoint':
        OmniboxPopupContextualEntrypointElement;
  }
}

customElements.define(
    OmniboxPopupContextualEntrypointElement.is,
    OmniboxPopupContextualEntrypointElement);
