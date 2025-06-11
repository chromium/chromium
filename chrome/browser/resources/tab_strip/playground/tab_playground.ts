// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '../alert_indicators.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getFavicon} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isRTL} from 'chrome://resources/js/util.js';

import type {AlertIndicatorsElement} from '../alert_indicators.js';
import {getTemplate} from '../tab.html.js';
import type {Tab, NodeId} from '../tab_strip_api.mojom-webui.js';
import {TabNetworkState} from '../tabs.mojom-webui.js';

import type {TabStripApiProxy} from './tab_strip_api.js';
import {TabStripApiProxyImpl} from './tab_strip_api.js';

function getPaddingInlineEndProperty(): string {
  return isRTL() ? 'paddingLeft' : 'paddingRight';
}

export class TabElement extends CustomElement {
  static override get template() {
    return getTemplate();
  }

  private alertIndicatorsEl_: AlertIndicatorsElement;
  private closeButtonEl_: HTMLElement;
  private dragImageEl_: HTMLElement;
  private tabEl_: HTMLElement;
  private faviconEl_: HTMLElement;
  private thumbnail_: HTMLImageElement;
  private tab_: Tab|null = null;
  private titleTextEl_: HTMLElement;
  private onTabActivating_: (tabId: NodeId) => void;
  private tabStripApi_: TabStripApiProxy;
  private isValidDragOverTarget_: boolean;
  private dragHandler_: any;

  // Temp public
  isActive: boolean = false;
  isPinned: boolean = false;
  blocked: boolean = false;
  crashed: boolean = false;
  showIcon: boolean = false;

  constructor() {
    super();

    this.alertIndicatorsEl_ =
        this.getRequiredElement('tabstrip-alert-indicators');
    // Normally, custom elements will get upgraded automatically once added
    // to the DOM, but TabElement may need to update properties on
    // AlertIndicatorElement before this happens, so upgrade it manually.
    customElements.upgrade(this.alertIndicatorsEl_);

    this.closeButtonEl_ = this.getRequiredElement('#close');
    this.closeButtonEl_.setAttribute(
        'aria-label', loadTimeData.getString('closeTab'));

    this.dragImageEl_ = this.getRequiredElement('#dragImage');
    this.tabEl_ = this.getRequiredElement('#tab');
    this.faviconEl_ = this.getRequiredElement('#favicon');
    this.thumbnail_ =
        this.getRequiredElement<HTMLImageElement>('#thumbnailImg');

    this.titleTextEl_ = this.getRequiredElement('#titleText');
    this.tabStripApi_ = TabStripApiProxyImpl.getInstance();
    this.dragHandler_ = () => 0;

    /**
     * Flag indicating if this TabElement can accept dragover events. This
     * is used to pause dragover events while animating as animating causes
     * the elements below the pointer to shift.
     */
    this.isValidDragOverTarget_ = true;


    this.tabEl_.addEventListener('click', () => this.onClick_());
    this.addEventListener(
        'dragend',
        (event: MouseEvent) => this.dragHandler_(this, event.clientX));

    this.closeButtonEl_.addEventListener('click', e => this.onClose_(e));
    this.onTabActivating_ = (tabId: NodeId) =>
        this.tabStripApi_.activateTab(tabId);
  }

  hasTabModel(): boolean {
    return this.tab_ !== null;
  }

  get tab(): Tab {
    assert(this.tab_);
    return this.tab_;
  }

  set dragEndHandler(handler: (element: TabElement, x: number) => void) {
    this.dragHandler_ = handler;
  }

  set tab(tab: Tab) {
    this.toggleAttribute('active', this.isActive);
    this.toggleAttribute('hide-icon_', !this.showIcon);
    this.toggleAttribute(
        'waiting_', tab.networkState === TabNetworkState.kWaiting);
    this.toggleAttribute(
        'loading_', tab.networkState === TabNetworkState.kLoading);
    this.toggleAttribute('pinned', this.isPinned);
    this.toggleAttribute('blocked_', this.blocked);
    this.setAttribute('draggable', String(true));
    this.toggleAttribute('crashed_', this.crashed);

    if (tab.title) {
      this.titleTextEl_.textContent = tab.title;
    } else if ((tab.networkState === TabNetworkState.kWaiting ||
                tab.networkState === TabNetworkState.kLoading)) {
      this.titleTextEl_.textContent = loadTimeData.getString('loadingTab');
    } else {
      this.titleTextEl_.textContent = loadTimeData.getString('defaultTabTitle');
    }
    this.titleTextEl_.setAttribute('aria-label', tab.title);

    if (tab.networkState === TabNetworkState.kWaiting) {
      this.faviconEl_.style.backgroundImage = 'none';
    } else if (tab.faviconUrl) {
      this.faviconEl_.style.backgroundImage = `url(${tab.faviconUrl.url})`;
    } else {
      this.faviconEl_.style.backgroundImage = getFavicon('');
    }

    // Expose the ID to an attribute to allow easy querySelector use
    this.setAttribute('data-tab-id', tab.id.id.toString());

    this.alertIndicatorsEl_.updateAlertStates(tab.alertStates)
        .then((alertIndicatorsCount) => {
          this.toggleAttribute('has-alert-states_', alertIndicatorsCount > 0);
        });

    this.tab_ = Object.freeze(tab);
  }

  get isValidDragOverTarget(): boolean {
    return !this.hasAttribute('dragging_') && this.isValidDragOverTarget_;
  }

  set isValidDragOverTarget(isValid: boolean) {
    this.isValidDragOverTarget_ = isValid;
  }

  override focus() {
    this.tabEl_.focus();
  }

  getDragImage(): HTMLElement {
    return this.dragImageEl_;
  }

  getDragImageCenter(): HTMLElement {
    // dragImageEl_ has padding, so the drag image should be centered relative
    // to tabEl_, the element within the padding.
    return this.tabEl_;
  }

  updateThumbnail(imgData: string) {
    this.thumbnail_.src = imgData;
  }

  private onClick_() {
    if (this.tab_) {  // Check if this.tab_ is not null
      this.onTabActivating_(this.tab_.id);
    } else {
      // Optionally, handle the case where this.tab_ is null,
      // for example, by logging an error or doing nothing.
      console.warn('Tab data is not available for onClick event.');
    }
  }

  private onClose_(event: Event) {
    assert(this.tab_);
    event.stopPropagation();
    console.info('Close tab', this.tab_.id);
    this.tabStripApi_.closeTabs([this.tab_.id]);
  }

  slideOut(): Promise<void> {
    assert(this.tab_);
    return new Promise(resolve => {
      const finishCallback = () => {
        this.remove();
        resolve();
      };

      this.animate(
          {
            transform: ['translateY(0)', 'translateY(-100%)'],
          },
          {
            duration: 150,
            easing: 'cubic-bezier(.4, 0, 1, 1)',
            fill: 'forwards',
          });
      this.animate(
          {
            opacity: [1, 0],
          },
          {
            delay: 97.5,
            duration: 50,
            fill: 'forwards',
          });

      const widthAnimationKeyframes = {
        maxWidth: ['var(--tabstrip-tab-width)', 0],
        [getPaddingInlineEndProperty()]: ['var(--tabstrip-tab-spacing)', 0],
      };

      const widthAnimation = this.animate(widthAnimationKeyframes as any, {
        delay: 97.5,
        duration: 300,
        easing: 'cubic-bezier(.4, 0, 0, 1)',
        fill: 'forwards',
      });

      const visibilityChangeListener = () => {
        console.info('Visibility change listener triggered');
      };

      document.addEventListener(
          'visibilitychange', visibilityChangeListener, {once: true});
      // The onfinish handler is put on the width animation, as it will end
      // last.
      widthAnimation.onfinish = () => {
        document.removeEventListener(
            'visibilitychange', visibilityChangeListener);
        finishCallback();
      };
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-tab-playground': TabElement;
  }
}

customElements.define('tabstrip-tab-playground', TabElement);

export function isTabElement(element: Element): boolean {
  return element.tagName === 'TABSTRIP-TAB-PLAYGROUND';
}
