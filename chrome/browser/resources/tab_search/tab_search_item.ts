// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/mwb_shared_icons.js';
import 'chrome://resources/cr_elements/mwb_shared_vars.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './strings.m.js';

import {MouseHoverableMixin} from 'chrome://resources/cr_elements/mouse_hoverable_mixin.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {get as deepGet, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ariaLabel, TabData, TabItemType} from './tab_data.js';
import {colorName} from './tab_group_color_helper.js';
import {Tab, TabGroup} from './tab_search.mojom-webui.js';
import {highlightText} from './tab_search_utils.js';
import {TabAlertState} from './tabs.mojom-webui.js';

export interface TabSearchItem {
  $: {
    groupTitle: HTMLElement,
    primaryText: HTMLElement,
    secondaryText: HTMLElement,
  };
}

const TabSearchItemBase = MouseHoverableMixin(PolymerElement);


export class TabSearchItem extends TabSearchItemBase {
  static get is() {
    return 'tab-search-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      data: {
        type: Object,
        observer: 'dataChanged_',
      },

      buttonRipples_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('useRipples'),
      },

      index: Number,
    };
  }

  data: TabData;
  private buttonRipples_: boolean;
  index: number;

  /**
   * @return Whether a close action can be performed on the item.
   */
  private isCloseable_(type: TabItemType): boolean {
    return type === TabItemType.OPEN_TAB;
  }

  private onItemClose_(e: Event) {
    this.dispatchEvent(new CustomEvent('close'));
    e.stopPropagation();
  }

  private faviconUrl_(tab: Tab): string {
    return tab.faviconUrl ?
        `url("${tab.faviconUrl.url}")` :
        getFaviconForPageURL(
            tab.isDefaultFavicon ? 'chrome://newtab' : tab.url.url, false);
  }

  /**
   * Determines the display attribute value for the group SVG element.
   */
  private groupSvgDisplay_(tabData: TabData): string {
    return tabData.tabGroup ? 'block' : 'none';
  }

  private isOpenTabAndHasMediaAlert_(tabData: TabData): boolean {
    if (tabData.type != TabItemType.OPEN_TAB ||
        !(tabData.tab as Tab).alertStates ||
        (tabData.tab as Tab).alertStates.length == 0) {
      return false;
    }

    /* Current UI mocks only have specs for the following media related alert
     * states. */
    function validAlertState(alert: TabAlertState): boolean {
      return alert == TabAlertState.kMediaRecording ||
          alert == TabAlertState.kAudioPlaying ||
          alert == TabAlertState.kAudioMuting;
    }

    return (tabData.tab as Tab).alertStates.some(validAlertState);
  }

  /**
   * Determines the display attribute value for the media indicator.
   */
  private mediaAlertVisibility_(tabData: TabData): string {
    return this.isOpenTabAndHasMediaAlert_(tabData) ? 'block' : 'none';
  }

  /**
   * Returns the correct media alert indicator class name.
   */
  private getMediaAlertImageClass_(tabData: TabData): string {
    if (!this.isOpenTabAndHasMediaAlert_(tabData)) {
      return '';
    }
    for (const alert of (tabData.tab as Tab).alertStates) {
      // Ordered in the same priority as GetTabAlertStatesForContents.
      if (alert == TabAlertState.kMediaRecording) {
        return 'media-recording';
      } else if (alert == TabAlertState.kAudioPlaying) {
        return 'audio-playing';
      } else if (alert == TabAlertState.kAudioMuting) {
        return 'audio-muting';
      }
    }

    return '';
  }

  private hasTabGroupWithTitle_(tabData: TabData): boolean {
    return !!(tabData.tabGroup && tabData.tabGroup.title);
  }

  private dataChanged_(data: TabData) {
    ([
      ['tab.title', this.$.primaryText],
      ['hostname', this.$.secondaryText],
      ['tabGroup.title', this.$.groupTitle],
    ] as Array<[string, HTMLElement]>)
        .forEach(([path, element]) => {
          if (element) {
            const highlightRanges =
                data.highlightRanges ? data.highlightRanges[path] : undefined;
            highlightText(element, deepGet(data, path), highlightRanges);
          }
        });

    // Show chrome:// if it's a chrome internal url
    let secondaryLabel = data.hostname;
    const protocol = new URL(data.tab.url.url).protocol;
    if (protocol === 'chrome:') {
      this.$.secondaryText.prepend(document.createTextNode('chrome://'));
      secondaryLabel = `chrome://${secondaryLabel}`;
    }

    if (data.tabGroup) {
      this.style.setProperty(
          '--group-dot-color',
          `var(--tab-group-color-${colorName(data.tabGroup.color)})`);
    }
  }

  ariaLabelForText_(tabData: TabData): string {
    return ariaLabel(tabData);
  }

  private ariaLabelForButton_(title: string): string {
    return `${loadTimeData.getString('closeTab')} ${title}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tab-search-item': TabSearchItem;
  }
}

customElements.define(TabSearchItem.is, TabSearchItem);
