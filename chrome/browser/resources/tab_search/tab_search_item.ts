// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './strings.m.js';

import {MouseHoverableMixinLit} from 'chrome://resources/cr_elements/mouse_hoverable_mixin_lit.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {normalizeURL, TabData, TabItemType} from './tab_data.js';
import {colorName} from './tab_group_color_helper.js';
import type {Tab} from './tab_search.mojom-webui.js';
import {getCss} from './tab_search_item.css.js';
import {getHtml} from './tab_search_item.html.js';
import {highlightText, tabHasMediaAlerts} from './tab_search_utils.js';
import {TabAlertState} from './tabs.mojom-webui.js';


function deepGet(obj: Record<string, any>, path: string): any {
  let value: Record<string, any> = obj;

  const parts = path.split('.');
  for (const part of parts) {
    if (value[part] === undefined) {
      return undefined;
    }
    value = value[part];
  }

  return value;
}


export interface TabSearchItemElement {
  $: {
    primaryText: HTMLElement,
    secondaryText: HTMLElement,
  };
}

const TabSearchItemBase = MouseHoverableMixinLit(CrLitElement);


export class TabSearchItemElement extends TabSearchItemBase {
  static get is() {
    return 'tab-search-item';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
      buttonRipples_: {type: Boolean},
      inSuggestedGroup: {type: Boolean},
      hideUrl: {type: Boolean},
      closeButtonIcon: {type: String},

      compact: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  data: TabData = new TabData(
      {
        active: false,
        faviconUrl: null,
        groupId: null,
        alertStates: [],
        index: 0,
        isDefaultFavicon: false,
        lastActiveElapsedText: '',
        lastActiveTimeTicks: {internalValue: BigInt(0)},
        pinned: false,
        showIcon: false,
        tabId: 1,
        title: '',
        url: {url: ''},
      },
      TabItemType.OPEN_TAB, '');
  protected buttonRipples_: boolean = loadTimeData.getBoolean('useRipples');
  inSuggestedGroup: boolean = false;
  compact: boolean = false;
  hideUrl: boolean = false;
  closeButtonIcon: string = 'tab-search:close';

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('data')) {
      if (this.data.tabGroup) {
        this.style.setProperty(
            '--group-dot-color',
            `var(--tab-group-color-${colorName(this.data.tabGroup.color)})`);
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('data')) {
      this.dataChanged_();
    }
  }

  /**
   * @return Whether a close action can be performed on the item.
   */
  protected isCloseable_(): boolean {
    return this.data.type === TabItemType.OPEN_TAB;
  }

  /**
   * @return the class name for the close button including a second class to
   *     preallocate space for the close button even while hidden if the tab
   *     will display a media alert.
   */
  protected getButtonContainerStyles_(): string {
    return 'button-container' +
        (this.isOpenTabAndHasMediaAlert_() ? ' allocate-space-while-hidden' :
                                             '');
  }

  protected getCloseButtonRole_(): string {
    // If this tab search item is an option within a list, the button
    // should also be treated as an option in a list to ensure the correct
    // focus traversal behavior when a screenreader is on.
    return this.role === 'option' ? 'option' : 'button';
  }

  protected onItemClose_(e: Event) {
    this.dispatchEvent(new CustomEvent('close'));
    e.stopPropagation();
  }

  protected faviconUrl_(): string {
    const tab = this.data.tab;
    return (tab as Tab).faviconUrl ?
        `url("${(tab as Tab).faviconUrl!.url}")` :
        getFaviconForPageURL(
            (tab as Tab).isDefaultFavicon ? 'chrome://newtab' : tab.url.url,
            false);
  }

  /**
   * Determines the display attribute value for the group SVG element.
   */
  protected groupSvgDisplay_(): string {
    return this.data.tabGroup ? 'block' : 'none';
  }

  private isOpenTabAndHasMediaAlert_(): boolean {
    const tabData = this.data;
    return tabData.type === TabItemType.OPEN_TAB &&
        tabHasMediaAlerts(tabData.tab as Tab);
  }

  /**
   * Determines the display attribute value for the media indicator.
   */
  protected mediaAlertVisibility_(): string {
    return this.isOpenTabAndHasMediaAlert_() ? 'block' : 'none';
  }

  /**
   * Returns the correct media alert indicator class name.
   */
  protected getMediaAlertImageClass_(): string {
    if (!this.isOpenTabAndHasMediaAlert_()) {
      return '';
    }
    // GetTabAlertStatesForContents adds alert indicators in the order of their
    // priority. Only relevant media alerts are sent over mojo so the first
    // element in alertStates will be the highest priority media alert to
    // display.
    const alert = (this.data.tab as Tab).alertStates[0];
    switch (alert) {
      case TabAlertState.kMediaRecording:
        return 'media-recording';
      case TabAlertState.kAudioRecording:
        return 'audio-recording';
      case TabAlertState.kVideoRecording:
        return 'video-recording';
      case TabAlertState.kAudioPlaying:
        return 'audio-playing';
      case TabAlertState.kAudioMuting:
        return 'audio-muting';
      default:
        return '';
    }
  }

  protected hasTabGroupWithTitle_(): boolean {
    return !!(this.data.tabGroup && this.data.tabGroup.title);
  }

  private dataChanged_() {
    const data = this.data;
    ([
      ['tab.title', this.$.primaryText],
      ['hostname', this.$.secondaryText],
      ['tabGroup.title', this.shadowRoot!.querySelector('#groupTitle')],
    ] as Array<[string, HTMLElement | null]>)
        .forEach(([path, element]) => {
          if (element) {
            const highlightRanges =
                data.highlightRanges ? data.highlightRanges[path] : undefined;
            highlightText(element, deepGet(data, path), highlightRanges);
          }
        });

    // Show chrome:// if it's a chrome internal url
    const protocol = new URL(normalizeURL(data.tab.url.url)).protocol;
    if (protocol === 'chrome:') {
      this.$.secondaryText.prepend(document.createTextNode('chrome://'));
    }
  }

  protected ariaLabelForButton_(): string {
    const title = this.data.tab.title;
    if (this.inSuggestedGroup) {
      return loadTimeData.getStringF('tabOrganizationCloseTabAriaLabel', title);
    }
    return `${loadTimeData.getString('closeTab')} ${title}`;
  }

  protected tooltipForButton_(): string {
    if (this.inSuggestedGroup) {
      return loadTimeData.getString('tabOrganizationCloseTabTooltip');
    }
    return loadTimeData.getString('closeTab');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-item': TabSearchItemElement;
  }
}

customElements.define(TabSearchItemElement.is, TabSearchItemElement);
