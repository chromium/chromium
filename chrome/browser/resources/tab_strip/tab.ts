// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './alert_indicators.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getFavicon} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isRTL} from 'chrome://resources/js/util.js';

import type {AlertIndicatorsElement} from './alert_indicators.js';
import {getTemplate} from './tab.html.js';
import type {Tab} from './tab_strip.mojom-webui.js';
import {TabNetworkState} from './tab_strip.mojom-webui.js';
import {TabSwiper} from './tab_swiper.js';
import type {TabsApiProxy} from './tabs_api_proxy.js';
import {CloseTabAction, TabsApiProxyImpl} from './tabs_api_proxy.js';

function getAccessibleTitle(tab: Tab): string {
  const tabTitle = tab.title;

  if (tab.crashed) {
    return loadTimeData.getStringF('tabCrashed', tabTitle);
  }

  if (tab.networkState === TabNetworkState.kError) {
    return loadTimeData.getStringF('tabNetworkError', tabTitle);
  }

  return tabTitle;
}

/**
 * TODO(crbug.com/40659171): padding-inline-end cannot be animated yet.
 */
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
  private tabsApi_: TabsApiProxy;
  private titleTextEl_: HTMLElement;
  private isValidDragOverTarget_: boolean;
  private tabSwiper_: TabSwiper;
  private onTabActivating_: (tabId: number) => void;

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

    this.tabsApi_ = TabsApiProxyImpl.getInstance();

    this.titleTextEl_ = this.getRequiredElement('#titleText');

    /**
     * Flag indicating if this TabElement can accept dragover events. This
     * is used to pause dragover events while animating as animating causes
     * the elements below the pointer to shift.
     */
    this.isValidDragOverTarget_ = true;

    this.tabEl_.addEventListener('click', () => this.onClick_());
    this.tabEl_.addEventListener('contextmenu', e => this.onContextMenu_(e));
    this.tabEl_.addEventListener('keydown', e => this.onKeyDown_(e));
    this.tabEl_.addEventListener('pointerup', e => this.onPointerUp_(e));

    this.closeButtonEl_.addEventListener('click', e => this.onClose_(e));
    this.addEventListener('swipe', () => this.onSwipe_());

    this.tabSwiper_ = new TabSwiper(this);

    this.onTabActivating_ = (_tabId: number) => {};
  }

  hasTabModel(): boolean {
    return this.tab_ !== null;
  }

  get tab(): Tab {
    assert(this.tab_);
    return this.tab_;
  }

  set tab(tab: Tab) {
    this.toggleAttribute('active', tab.active);
    this.tabEl_.setAttribute('aria-selected', tab.active.toString());
    this.toggleAttribute('hide-icon_', !tab.showIcon);
    this.toggleAttribute(
        'waiting_',
        !tab.shouldHideThrobber &&
            tab.networkState === TabNetworkState.kWaiting);
    this.toggleAttribute(
        'loading_',
        !tab.shouldHideThrobber &&
            tab.networkState === TabNetworkState.kLoading);
    this.toggleAttribute('pinned', tab.pinned);
    this.toggleAttribute('blocked_', tab.blocked);
    this.setAttribute('draggable', String(true));
    this.toggleAttribute('crashed_', tab.crashed);

    if (tab.title) {
      this.titleTextEl_.textContent = tab.title;
    } else if (
        !tab.shouldHideThrobber &&
        (tab.networkState === TabNetworkState.kWaiting ||
         tab.networkState === TabNetworkState.kLoading)) {
      this.titleTextEl_.textContent = loadTimeData.getString('loadingTab');
    } else {
      this.titleTextEl_.textContent = loadTimeData.getString('defaultTabTitle');
    }
    this.titleTextEl_.setAttribute('aria-label', getAccessibleTitle(tab));

    if (tab.networkState === TabNetworkState.kWaiting ||
        (tab.networkState === TabNetworkState.kLoading &&
         tab.isDefaultFavicon)) {
      this.faviconEl_.style.backgroundImage = 'none';
    } else if (tab.faviconUrl) {
      this.faviconEl_.style.backgroundImage = `url(${
          tab.active && tab.activeFaviconUrl ? tab.activeFaviconUrl.url :
                                               tab.faviconUrl.url})`;
    } else {
      this.faviconEl_.style.backgroundImage = getFavicon('');
    }

    // Expose the ID to an attribute to allow easy querySelector use
    this.setAttribute('data-tab-id', tab.id.toString());

    this.alertIndicatorsEl_.updateAlertStates(tab.alertStates)
        .then((alertIndicatorsCount) => {
          this.toggleAttribute('has-alert-states_', alertIndicatorsCount > 0);
        });

    if (!this.tab_ || (this.tab_.pinned !== tab.pinned && !tab.pinned)) {
      this.tabSwiper_.startObserving();
    } else if (this.tab_.pinned !== tab.pinned && tab.pinned) {
      this.tabSwiper_.stopObserving();
    }

    this.tab_ = Object.freeze(tab);
  }

  get isValidDragOverTarget(): boolean {
    return !this.hasAttribute('dragging_') && this.isValidDragOverTarget_;
  }

  set isValidDragOverTarget(isValid: boolean) {
    this.isValidDragOverTarget_ = isValid;
  }

  set onTabActivating(callback: (tabId: number) => void) {
    this.onTabActivating_ = callback;
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
    if (!this.tab_ || this.tabSwiper_.wasSwiping()) {
      return;
    }

    const tabId = this.tab_.id;
    this.onTabActivating_(tabId);
    this.tabsApi_.activateTab(tabId);

    this.setTouchPressed(false);
    this.tabsApi_.closeContainer();
  }

  private onContextMenu_(event: Event) {
    event.preventDefault();
    event.stopPropagation();
  }

  private onClose_(event: Event) {
    assert(this.tab_);
    event.stopPropagation();
    this.tabsApi_.closeTab(this.tab_.id, CloseTabAction.CLOSE_BUTTON);
  }

  private onSwipe_() {
    assert(this.tab_);
    this.tabsApi_.closeTab(this.tab_.id, CloseTabAction.SWIPED_TO_CLOSE);
  }

  private onKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter' || event.key === ' ') {
      this.onClick_();
    }
  }

  private onPointerUp_(event: PointerEvent) {
    event.stopPropagation();
    if (event.pointerType !== 'touch' && event.button === 2) {
      this.tabsApi_.showTabContextMenu(
          this.tab.id, event.clientX, event.clientY);
    }
  }

  resetSwipe() {
    this.tabSwiper_.reset();
  }

  setDragging(isDragging: boolean) {
    this.toggleAttribute('dragging_', isDragging);
  }

  setDraggedOut(isDraggedOut: boolean) {
    this.toggleAttribute('dragged-out_', isDraggedOut);
  }

  isDraggedOut(): boolean {
    return this.hasAttribute('dragged-out_');
  }

  setTouchPressed(isTouchPressed: boolean) {
    this.toggleAttribute('touch_pressed_', isTouchPressed);
  }

  slideOut(): Promise<void> {
    assert(this.tab_);
    if (!this.tabsApi_.isVisible() || this.tab_.pinned ||
        this.tabSwiper_.wasSwiping()) {
      this.remove();
      return Promise.resolve();
    }

    return new Promise(resolve => {
      const finishCallback = () => {
        this.remove();
        resolve();
      };

      const translateAnimation = this.animate(
          {
            transform: ['translateY(0)', 'translateY(-100%)'],
          },
          {
            duration: 150,
            easing: 'cubic-bezier(.4, 0, 1, 1)',
            fill: 'forwards',
          });
      const opacityAnimation = this.animate(
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
      // TODO(dpapad): Figure out why TypeScript compiler does not understand
      // the alternative keyframe syntax. Seems to work in the TS playground.
      const widthAnimation = this.animate(widthAnimationKeyframes as any, {
        delay: 97.5,
        duration: 300,
        easing: 'cubic-bezier(.4, 0, 0, 1)',
        fill: 'forwards',
      });

      const visibilityChangeListener = () => {
        if (!this.tabsApi_.isVisible()) {
          // If a tab strip becomes hidden during the animation, the onfinish
          // event will not get fired until the tab strip becomes visible again.
          // Therefore, when the tab strip becomes hidden, immediately call the
          // finish callback.
          translateAnimation.cancel();
          opacityAnimation.cancel();
          widthAnimation.cancel();
          finishCallback();
        }
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
    'tabstrip-tab': TabElement;
  }
}

customElements.define('tabstrip-tab', TabElement);

export function isTabElement(element: Element): boolean {
  return element.tagName === 'TABSTRIP-TAB';
}
