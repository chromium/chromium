// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {getFavicon} from 'chrome://resources/js/icon.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';

import {AlertIndicatorsElement} from './alert_indicators.js';
import {CustomElement} from './custom_element.js';
import {TabStripEmbedderProxy, TabStripEmbedderProxyImpl} from './tab_strip_embedder_proxy.js';
import {TabSwiper} from './tab_swiper.js';
import {CloseTabAction, TabData, TabNetworkState, TabsApiProxy, TabsApiProxyImpl} from './tabs_api_proxy.js';

const DEFAULT_ANIMATION_DURATION = 125;

/**
 * @param {!TabData} tab
 * @return {string}
 */
function getAccessibleTitle(tab) {
  const tabTitle = tab.title;

  if (tab.crashed) {
    return loadTimeData.getStringF('tabCrashed', tabTitle);
  }

  if (tab.networkState === TabNetworkState.ERROR) {
    return loadTimeData.getStringF('tabNetworkError', tabTitle);
  }

  return tabTitle;
}

/**
 * TODO(crbug.com/1025390): padding-inline-end cannot be animated yet.
 * @return {string}
 */
function getPaddingInlineEndProperty() {
  return isRTL() ? 'paddingLeft' : 'paddingRight';
}

export class TabElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    this.alertIndicatorsEl_ = /** @type {!AlertIndicatorsElement} */
        (this.$('tabstrip-alert-indicators'));
    // Normally, custom elements will get upgraded automatically once added to
    // the DOM, but TabElement may need to update properties on
    // AlertIndicatorElement before this happens, so upgrade it manually.
    customElements.upgrade(this.alertIndicatorsEl_);

    /** @private {!HTMLElement} */
    this.closeButtonEl_ = /** @type {!HTMLElement} */ (this.$('#close'));
    this.closeButtonEl_.setAttribute(
        'aria-label', loadTimeData.getString('closeTab'));

    /** @private {!HTMLElement} */
    this.dragImageEl_ = /** @type {!HTMLElement} */ (this.$('#dragImage'));

    /** @private {!HTMLElement} */
    this.tabEl_ = /** @type {!HTMLElement} */ (this.$('#tab'));

    /** @private {!HTMLElement} */
    this.faviconEl_ = /** @type {!HTMLElement} */ (this.$('#favicon'));

    /** @private {!HTMLElement} */
    this.thumbnailContainer_ =
        /** @type {!HTMLElement} */ (this.$('#thumbnail'));

    /** @private {!Image} */
    this.thumbnail_ = /** @type {!Image} */ (this.$('#thumbnailImg'));

    /** @private {!TabData} */
    this.tab_;

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxyImpl.getInstance();

    /** @private {!TabStripEmbedderProxy} */
    this.embedderApi_ = TabStripEmbedderProxyImpl.getInstance();

    /** @private {!HTMLElement} */
    this.titleTextEl_ = /** @type {!HTMLElement} */ (this.$('#titleText'));

    /**
     * Flag indicating if this TabElement can accept dragover events. This
     * is used to pause dragover events while animating as animating causes
     * the elements below the pointer to shift.
     * @private {boolean}
     */
    this.isValidDragOverTarget_ = true;

    this.tabEl_.addEventListener('click', () => this.onClick_());
    this.tabEl_.addEventListener('contextmenu', e => this.onContextMenu_(e));
    this.tabEl_.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
    this.tabEl_.addEventListener(
        'pointerup', e => this.onPointerUp_(/** @type {!PointerEvent} */ (e)));

    this.closeButtonEl_.addEventListener('click', e => this.onClose_(e));
    this.addEventListener('swipe', () => this.onSwipe_());

    /** @private @const {!TabSwiper} */
    this.tabSwiper_ = new TabSwiper(this);

    /** @private {!Function} */
    this.onTabActivating_ = (tabId) => {};
  }

  /** @return {!TabData} */
  get tab() {
    return this.tab_;
  }

  /** @param {!TabData} tab */
  set tab(tab) {
    assert(this.tab_ !== tab);
    this.toggleAttribute('active', tab.active);
    this.tabEl_.setAttribute('aria-selected', tab.active.toString());
    this.toggleAttribute('hide-icon_', !tab.showIcon);
    this.toggleAttribute(
        'waiting_',
        !tab.shouldHideThrobber &&
            tab.networkState === TabNetworkState.WAITING);
    this.toggleAttribute(
        'loading_',
        !tab.shouldHideThrobber &&
            tab.networkState === TabNetworkState.LOADING);
    this.toggleAttribute('pinned', tab.pinned);
    this.toggleAttribute('blocked_', tab.blocked);
    this.setAttribute('draggable', true);
    this.toggleAttribute('crashed_', tab.crashed);

    if (tab.title) {
      this.titleTextEl_.textContent = tab.title;
    } else if (
        !tab.shouldHideThrobber &&
        (tab.networkState === TabNetworkState.WAITING ||
         tab.networkState === TabNetworkState.LOADING)) {
      this.titleTextEl_.textContent = loadTimeData.getString('loadingTab');
    } else {
      this.titleTextEl_.textContent = loadTimeData.getString('defaultTabTitle');
    }
    this.titleTextEl_.setAttribute('aria-label', getAccessibleTitle(tab));

    if (tab.networkState === TabNetworkState.WAITING ||
        (tab.networkState === TabNetworkState.LOADING &&
         tab.isDefaultFavicon)) {
      this.faviconEl_.style.backgroundImage = 'none';
    } else if (tab.favIconUrl) {
      this.faviconEl_.style.backgroundImage = `url(${tab.favIconUrl})`;
    } else {
      this.faviconEl_.style.backgroundImage = getFavicon('');
    }

    // Expose the ID to an attribute to allow easy querySelector use
    this.setAttribute('data-tab-id', tab.id);

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

  /** @return {boolean} */
  get isValidDragOverTarget() {
    return !this.hasAttribute('dragging_') && this.isValidDragOverTarget_;
  }

  /** @param {boolean} isValid */
  set isValidDragOverTarget(isValid) {
    this.isValidDragOverTarget_ = isValid;
  }

  /** @param {!Function} callback */
  set onTabActivating(callback) {
    this.onTabActivating_ = callback;
  }

  focus() {
    this.tabEl_.focus();
  }

  /** @return {!HTMLElement} */
  getDragImage() {
    return this.dragImageEl_;
  }

  /** @return {!HTMLElement} */
  getDragImageCenter() {
    // dragImageEl_ has padding, so the drag image should be centered relative
    // to tabEl_, the element within the padding.
    return this.tabEl_;
  }

  /**
   * @param {string} imgData
   */
  updateThumbnail(imgData) {
    this.thumbnail_.src = imgData;
  }

  /** @private */
  onClick_() {
    if (!this.tab_ || this.tabSwiper_.wasSwiping()) {
      return;
    }

    const tabId = this.tab_.id;
    this.onTabActivating_(tabId);
    this.tabsApi_.activateTab(tabId);

    this.embedderApi_.closeContainer();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContextMenu_(event) {
    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * @param {!Event} event
   * @private
   */
  onClose_(event) {
    assert(this.tab_);
    event.stopPropagation();
    this.tabsApi_.closeTab(this.tab_.id, CloseTabAction.CLOSE_BUTTON);
  }

  /** @private */
  onSwipe_() {
    assert(this.tab_);
    this.tabsApi_.closeTab(this.tab_.id, CloseTabAction.SWIPED_TO_CLOSE);
  }

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeyDown_(event) {
    if (event.key === 'Enter' || event.key === ' ') {
      this.onClick_();
    }
  }

  /**
   * @param {!PointerEvent} event
   * @private
   */
  onPointerUp_(event) {
    if (event.pointerType !== 'touch' && event.button === 2) {
      this.embedderApi_.showTabContextMenu(
          this.tab.id, event.clientX, event.clientY);
    }
  }

  resetSwipe() {
    this.tabSwiper_.reset();
  }

  /**
   * @param {boolean} isDragging
   */
  setDragging(isDragging) {
    this.toggleAttribute('dragging_', isDragging);
  }

  /** @param {boolean} isDraggedOut */
  setDraggedOut(isDraggedOut) {
    this.toggleAttribute('dragged-out_', isDraggedOut);
  }

  /**
   * @return {!Promise}
   */
  slideIn() {
    const paddingInlineEnd = getPaddingInlineEndProperty();

    // If this TabElement is the last tab, there needs to be enough space for
    // the view to scroll to it. Therefore, immediately take up all the space
    // it needs to and only animate the scale.
    const isLastChild = this.nextElementSibling === null;

    const startState = {
      maxWidth: isLastChild ? 'var(--tabstrip-tab-width)' : 0,
      transform: `scale(0)`,
    };
    startState[paddingInlineEnd] =
        isLastChild ? 'var(--tabstrip-tab-spacing)' : 0;

    const finishState = {
      maxWidth: `var(--tabstrip-tab-width)`,
      transform: `scale(1)`,
    };
    finishState[paddingInlineEnd] = 'var(--tabstrip-tab-spacing)';

    return new Promise(resolve => {
      const animation = this.animate([startState, finishState], {
        duration: 300,
        easing: 'cubic-bezier(.4, 0, 0, 1)',
      });
      animation.onfinish = () => {
        resolve();
      };

      // TODO(crbug.com/1035678) By the next animation frame, the animation
      // should start playing. By the time another animation frame happens,
      // force play the animation if the animation has not yet begun. Remove
      // if/when the Blink issue has been fixed.
      requestAnimationFrame(() => {
        requestAnimationFrame(() => {
          if (animation.pending) {
            animation.play();
          }
        });
      });
    });
  }

  /**
   * @return {!Promise}
   */
  slideOut() {
    if (!this.embedderApi_.isVisible() || this.tab_.pinned ||
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
      };
      widthAnimationKeyframes[getPaddingInlineEndProperty()] =
          ['var(--tabstrip-tab-spacing)', 0];
      const widthAnimation = this.animate(widthAnimationKeyframes, {
        delay: 97.5,
        duration: 300,
        easing: 'cubic-bezier(.4, 0, 0, 1)',
        fill: 'forwards',
      });

      const visibilityChangeListener = () => {
        if (!this.embedderApi_.isVisible()) {
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

customElements.define('tabstrip-tab', TabElement);

/**
 * @param {!Element} element
 * @return {boolean}
 */
export function isTabElement(element) {
  return element.tagName === 'TABSTRIP-TAB';
}
