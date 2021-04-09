// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './tab.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {addWebUIListener, removeWebUIListener, WebUIListener} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';

import {DragManager, DragManagerDelegate} from './drag_manager.js';
import {isTabElement, TabElement} from './tab.js';
import {isTabGroupElement, TabGroupElement} from './tab_group.js';
import {TabStripEmbedderProxy, TabStripEmbedderProxyImpl} from './tab_strip_embedder_proxy.js';
import {TabData, TabGroupVisualData, TabsApiProxy, TabsApiProxyImpl} from './tabs_api_proxy.js';

/**
 * The amount of padding to leave between the edge of the screen and the active
 * tab when auto-scrolling. This should leave some room to show the previous or
 * next tab to afford to users that there more tabs if the user scrolls.
 * @const {number}
 */
const SCROLL_PADDING = 32;

/** @type {boolean} */
let scrollAnimationEnabled = true;

/** @const {number} */
const TOUCH_CONTEXT_MENU_OFFSET_X = 8;

/** @const {number} */
const TOUCH_CONTEXT_MENU_OFFSET_Y = -40;

/**
 * Context menu should position below the element for touch.
 * @param {!Element} element
 * @return {!Object<{x: number, y: number}>}
 */
function getContextMenuPosition(element) {
  const rect = element.getBoundingClientRect();
  return {
    x: rect.left + TOUCH_CONTEXT_MENU_OFFSET_X,
    y: rect.bottom + TOUCH_CONTEXT_MENU_OFFSET_Y
  };
}

/** @param {boolean} enabled */
export function setScrollAnimationEnabledForTesting(enabled) {
  scrollAnimationEnabled = enabled;
}

/**
 * @enum {string}
 */
const LayoutVariable = {
  VIEWPORT_WIDTH: '--tabstrip-viewport-width',
  NEW_TAB_BUTTON_MARGIN: '--tabstrip-new-tab-button-margin',
  NEW_TAB_BUTTON_WIDTH: '--tabstrip-new-tab-button-width',
  TAB_WIDTH: '--tabstrip-tab-thumbnail-width',
};

/**
 * Animates a series of elements to indicate that tabs have moved position.
 * @param {!Element} movedElement
 * @param {number} prevIndex
 * @param {number} newIndex
 */
function animateElementMoved(movedElement, prevIndex, newIndex) {
  // Direction is -1 for moving towards a lower index, +1 for moving
  // towards a higher index. If moving towards a lower index, the TabList needs
  // to animate everything from the movedElement's current index to its prev
  // index by traversing the nextElementSibling of each element because the
  // movedElement is now at a preceding position from all the elements it has
  // slid across. If moving towards a higher index, the TabList needs to
  // traverse the previousElementSiblings.
  const direction = Math.sign(newIndex - prevIndex);

  /**
   * @param {!Element} element
   * @return {?Element}
   */
  function getSiblingToAnimate(element) {
    return direction === -1 ? element.nextElementSibling :
                              element.previousElementSibling;
  }
  let elementToAnimate = getSiblingToAnimate(movedElement);
  for (let i = newIndex; i !== prevIndex && elementToAnimate; i -= direction) {
    const elementToAnimatePrevIndex = i;
    const elementToAnimateNewIndex = i - direction;
    slideElement(
        elementToAnimate, elementToAnimatePrevIndex, elementToAnimateNewIndex);
    elementToAnimate = getSiblingToAnimate(elementToAnimate);
  }

  slideElement(movedElement, prevIndex, newIndex);
}

/**
 * Animates the slide of an element across the tab strip (both vertically and
 * horizontally for pinned tabs, and horizontally for other tabs and groups).
 * @param {!Element} element
 * @param {number} prevIndex
 * @param {number} newIndex
 */
function slideElement(element, prevIndex, newIndex) {
  let horizontalMovement = newIndex - prevIndex;
  let verticalMovement = 0;

  if (isTabElement(element) && element.tab.pinned) {
    const pinnedTabsPerColumn = 3;
    const columnChange = Math.floor(newIndex / pinnedTabsPerColumn) -
        Math.floor(prevIndex / pinnedTabsPerColumn);
    horizontalMovement = columnChange;
    verticalMovement =
        (newIndex - prevIndex) - (columnChange * pinnedTabsPerColumn);
  }

  horizontalMovement *= isRTL() ? -1 : 1;

  const translateX = `calc(${horizontalMovement * -1} * ` +
      '(var(--tabstrip-tab-width) + var(--tabstrip-tab-spacing)))';
  const translateY = `calc(${verticalMovement * -1} * ` +
      '(var(--tabstrip-tab-height) + var(--tabstrip-tab-spacing)))';

  element.isValidDragOverTarget = false;
  const animation = element.animate(
      [
        {transform: `translate(${translateX}, ${translateY})`},
        {transform: 'translate(0, 0)'},
      ],
      {
        duration: 120,
        easing: 'ease-out',
      });
  function onComplete() {
    element.isValidDragOverTarget = true;
  }
  animation.oncancel = onComplete;
  animation.onfinish = onComplete;
}

/** @implements {DragManagerDelegate} */
export class TabListElement extends CustomElement {
  static get template() {
    return `{__html_template__}`;
  }

  constructor() {
    super();

    /**
     * A chain of promises that the tab list needs to keep track of. The chain
     * is useful in cases when the list needs to wait for all animations to
     * finish in order to get accurate pixels (such as getting the position of a
     * tab) or accurate element counts.
     * @type {!Promise}
     */
    this.animationPromises = Promise.resolve();

    /**
     * The ID of the current animation frame that is in queue to update the
     * scroll position.
     * @private {?number}
     */
    this.currentScrollUpdateFrame_ = null;

    /** @private {!Function} */
    this.documentVisibilityChangeListener_ = () =>
        this.onDocumentVisibilityChange_();

    /**
     * The element that is currently being dragged.
     * @private {!TabElement|!TabGroupElement|undefined}
     */
    this.draggedItem_;

    /** @private {!Element} */
    this.dropPlaceholder_ = document.createElement('div');
    this.dropPlaceholder_.id = 'dropPlaceholder';

    /** @private @const {!FocusOutlineManager} */
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);

    /**
     * Map of tab IDs to whether or not the tab's thumbnail should be tracked.
     * @private {!Map<number, boolean>}
     */
    this.thumbnailTracker_ = new Map();

    /**
     * An intersection observer is needed to observe which TabElements are
     * currently in view or close to being in view, which will help determine
     * which thumbnails need to be tracked to stay fresh and which can be
     * untracked until they become visible.
     * @private {!IntersectionObserver}
     */
    this.intersectionObserver_ = new IntersectionObserver(entries => {
      for (const entry of entries) {
        this.thumbnailTracker_.set(entry.target.tab.id, entry.isIntersecting);
      }

      if (this.scrollingTimeoutId_ === -1) {
        // If there is no need to wait for scroll to end, immediately process
        // and request thumbnails.
        this.flushThumbnailTracker_();
      }
    }, {
      root: this,
      // The horizontal root margin is set to 100% to also track thumbnails that
      // are one standard finger swipe away.
      rootMargin: '0% 100%',
    });

    /** @private {number|undefined} */
    this.activatingTabId_;

    /** @private {number|undefined} Timestamp in ms */
    this.activatingTabIdTimestamp_;

    /** @private @const {!EventTracker} */
    this.eventTracker_ = new EventTracker();

    /** @private {!TabElement|null} */
    this.lastTargetedTab_;

    /** @private {!Object<{x: number, y: number}>|undefined} */
    this.lastTouchPoint_;

    /** @private {!Element} */
    this.newTabButtonElement_ =
        /** @type {!Element} */ (this.$('#newTabButton'));

    /** @private {!Element} */
    this.pinnedTabsElement_ = /** @type {!Element} */ (this.$('#pinnedTabs'));

    /** @private {!TabStripEmbedderProxy} */
    this.tabStripEmbedderProxy_ = TabStripEmbedderProxyImpl.getInstance();

    /** @private {!TabsApiProxy} */
    this.tabsApi_ = TabsApiProxyImpl.getInstance();

    /** @private {!Element} */
    this.unpinnedTabsElement_ =
        /** @type {!Element} */ (this.$('#unpinnedTabs'));

    /** @private {!Array<!WebUIListener>} */
    this.webUIListeners_ = [];

    /** @private {!Function} */
    this.windowBlurListener_ = () => this.onWindowBlur_();

    /**
     * Timeout that is created at every scroll event and is either canceled at
     * each subsequent scroll event or resolves after a few milliseconds after
     * the last scroll event.
     * @private {number}
     */
    this.scrollingTimeoutId_ = -1;

    /** @private {!Function} */
    this.scrollListener_ = (e) => this.onScroll_(e);

    this.addWebUIListener_(
        'layout-changed', layout => this.applyCSSDictionary_(layout));
    this.addWebUIListener_('theme-changed', () => {
      this.fetchAndUpdateColors_();
      this.fetchAndUpdateGroupData_();
    });
    this.tabStripEmbedderProxy_.observeThemeChanges();

    this.addWebUIListener_(
        'tab-thumbnail-updated', this.tabThumbnailUpdated_.bind(this));

    this.addWebUIListener_('long-press', () => this.handleLongPress_());

    this.addWebUIListener_(
        'context-menu-closed', () => this.clearLastTargetedTab_());

    this.eventTracker_.add(
        document, 'contextmenu', e => this.onContextMenu_(e));
    this.eventTracker_.add(
        document, 'pointerup',
        e => this.onPointerUp_(/** @type {!PointerEvent} */ (e)));
    this.eventTracker_.add(
        document, 'visibilitychange', () => this.onDocumentVisibilityChange_());
    this.eventTracker_.add(window, 'blur', () => this.onWindowBlur_());
    this.eventTracker_.add(this, 'scroll', e => this.onScroll_(e));
    this.eventTracker_.add(
        document, 'touchstart', (e) => this.onTouchStart_(e));
    // Touchend events happen when a touch gesture finishes normally (ie not due
    // to the context menu appearing or drag starting). Clear the last targeted
    // tab on a drag end to ensure `lastTargetedTab_` is cleared for the cases
    // that do not end with a dragstart or the context menu appearing.
    this.eventTracker_.add(
        document, 'touchend', () => this.clearLastTargetedTab_());
    this.addWebUIListener_(
        'received-keyboard-focus', () => this.onReceivedKeyboardFocus_());

    this.newTabButtonElement_.addEventListener('click', () => {
      this.tabsApi_.createNewTab();
    });

    const dragManager = new DragManager(this);
    dragManager.startObserving();
  }

  /**
   * @param {!Promise} promise
   * @private
   */
  addAnimationPromise_(promise) {
    this.animationPromises = this.animationPromises.then(() => promise);
  }

  /**
   * @param {string} eventName
   * @param {!Function} callback
   * @private
   */
  addWebUIListener_(eventName, callback) {
    this.webUIListeners_.push(addWebUIListener(eventName, callback));
  }

  /**
   * @param {number} scrollBy
   * @private
   */
  animateScrollPosition_(scrollBy) {
    if (this.currentScrollUpdateFrame_) {
      cancelAnimationFrame(this.currentScrollUpdateFrame_);
      this.currentScrollUpdateFrame_ = null;
    }

    const prevScrollLeft = this.scrollLeft;
    if (!scrollAnimationEnabled || !this.tabStripEmbedderProxy_.isVisible()) {
      // Do not animate if tab strip is not visible.
      this.scrollLeft = prevScrollLeft + scrollBy;
      return;
    }

    const duration = 350;
    let startTime;

    const onAnimationFrame = (currentTime) => {
      const startScroll = this.scrollLeft;
      if (!startTime) {
        startTime = currentTime;
      }

      const elapsedRatio = Math.min(1, (currentTime - startTime) / duration);

      // The elapsed ratio should be decelerated such that the elapsed time
      // of the animation gets less and less further apart as time goes on,
      // giving the effect of an animation that slows down towards the end. When
      // 0ms has passed, the decelerated ratio should be 0. When the full
      // duration has passed, the ratio should be 1.
      const deceleratedRatio =
          1 - (1 - elapsedRatio) / Math.pow(2, 6 * elapsedRatio);

      this.scrollLeft = prevScrollLeft + (scrollBy * deceleratedRatio);

      this.currentScrollUpdateFrame_ =
          deceleratedRatio < 1 ? requestAnimationFrame(onAnimationFrame) : null;
    };
    this.currentScrollUpdateFrame_ = requestAnimationFrame(onAnimationFrame);
  }

  /**
   * @param {!Object<string, string>} dictionary
   * @private
   */
  applyCSSDictionary_(dictionary) {
    for (const [cssVariable, value] of Object.entries(dictionary)) {
      this.style.setProperty(cssVariable, value);
    }
  }

  /** @private */
  clearScrollTimeout_() {
    clearTimeout(this.scrollingTimeoutId_);
    this.scrollingTimeoutId_ = -1;
  }

  connectedCallback() {
    this.tabStripEmbedderProxy_.getLayout().then(
        layout => this.applyCSSDictionary_(layout));
    this.fetchAndUpdateColors_();

    const getTabsStartTimestamp = Date.now();
    this.tabsApi_.getTabs().then(tabs => {
      this.tabStripEmbedderProxy_.reportTabDataReceivedDuration(
          tabs.length, Date.now() - getTabsStartTimestamp);

      const createTabsStartTimestamp = Date.now();
      tabs.forEach(tab => this.onTabCreated_(tab));
      this.fetchAndUpdateGroupData_();
      this.tabStripEmbedderProxy_.reportTabCreationDuration(
          tabs.length, Date.now() - createTabsStartTimestamp);

      this.addWebUIListener_(
          'show-context-menu', () => this.onShowContextMenu_());
      this.addWebUIListener_('tab-created', tab => this.onTabCreated_(tab));
      this.addWebUIListener_(
          'tab-moved',
          (tabId, newIndex, pinned) =>
              this.onTabMoved_(tabId, newIndex, pinned));
      this.addWebUIListener_('tab-removed', tabId => this.onTabRemoved_(tabId));
      this.addWebUIListener_(
          'tab-replaced', (oldId, newId) => this.onTabReplaced_(oldId, newId));
      this.addWebUIListener_('tab-updated', tab => this.onTabUpdated_(tab));
      this.addWebUIListener_(
          'tab-active-changed', tabId => this.onTabActivated_(tabId));
      this.addWebUIListener_(
          'tab-close-cancelled', tabId => this.onTabCloseCancelled_(tabId));
      this.addWebUIListener_(
          'tab-group-state-changed',
          (tabId, index, groupId) =>
              this.onTabGroupStateChanged_(tabId, index, groupId));
      this.addWebUIListener_(
          'tab-group-closed', groupId => this.onTabGroupClosed_(groupId));
      this.addWebUIListener_(
          'tab-group-moved',
          (groupId, index) => this.onTabGroupMoved_(groupId, index));
      this.addWebUIListener_(
          'tab-group-visuals-changed',
          (groupId, visualData) =>
              this.onTabGroupVisualsChanged_(groupId, visualData));
    });
  }

  disconnectedCallback() {
    this.webUIListeners_.forEach(removeWebUIListener);
    this.eventTracker_.removeAll();
  }

  /**
   * @param {!TabData} tab
   * @return {!TabElement}
   * @private
   */
  createTabElement_(tab) {
    const tabElement = new TabElement();
    tabElement.tab = tab;
    tabElement.onTabActivating = (id) => {
      this.onTabActivating_(id);
    };
    return tabElement;
  }

  /**
   * @param {number} tabId
   * @return {?TabElement}
   * @private
   */
  findTabElement_(tabId) {
    return /** @type {?TabElement} */ (
        this.$(`tabstrip-tab[data-tab-id="${tabId}"]`));
  }

  /**
   * @param {string} groupId
   * @return {?TabGroupElement}
   * @private
   */
  findTabGroupElement_(groupId) {
    return /** @type {?TabGroupElement} */ (
        this.$(`tabstrip-tab-group[data-group-id="${groupId}"]`));
  }

  /** @private */
  fetchAndUpdateColors_() {
    this.tabStripEmbedderProxy_.getColors().then(
        colors => this.applyCSSDictionary_(colors));
  }

  /** @private */
  fetchAndUpdateGroupData_() {
    const tabGroupElements = this.$all('tabstrip-tab-group');
    this.tabsApi_.getGroupVisualData().then(data => {
      tabGroupElements.forEach(tabGroupElement => {
        tabGroupElement.updateVisuals(
            assert(data[tabGroupElement.dataset.groupId]));
      });
    });
  }

  /**
   * @return {?TabElement}
   * @private
   */
  getActiveTab_() {
    return /** @type {?TabElement} */ (this.$('tabstrip-tab[active]'));
  }

  /**
   * @param {!TabElement} tabElement
   * @return {number}
   */
  getIndexOfTab(tabElement) {
    return Array.prototype.indexOf.call(this.$all('tabstrip-tab'), tabElement);
  }

  /**
   * @param {!LayoutVariable} variable
   * @return {number} in pixels
   */
  getLayoutVariable_(variable) {
    return parseInt(this.style.getPropertyValue(variable), 10);
  }

  /** @private */
  handleLongPress_() {
    if (this.lastTargetedTab_) {
      this.lastTargetedTab_.setTouchPressed(true);
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContextMenu_(event) {
    // Prevent the default context menu from triggering.
    event.preventDefault();
  }

  /**
   * @param {!PointerEvent} event
   * @private
   */
  onPointerUp_(event) {
    event.stopPropagation();
    if (event.pointerType !== 'touch' && event.button === 2) {
      // If processing an uncaught right click event show the background context
      // menu.
      this.tabStripEmbedderProxy_.showBackgroundContextMenu(
          event.clientX, event.clientY);
    }
  }

  /** @private */
  onDocumentVisibilityChange_() {
    if (!this.tabStripEmbedderProxy_.isVisible()) {
      this.scrollToActiveTab_();
    }

    this.unpinnedTabsElement_.childNodes.forEach(element => {
      if (isTabGroupElement(/** @type {!Element} */ (element))) {
        element.childNodes.forEach(
            tabElement => this.updateThumbnailTrackStatus_(
                /** @type {!TabElement} */ (tabElement)));
      } else {
        this.updateThumbnailTrackStatus_(
            /** @type {!TabElement} */ (element));
      }
    });
  }

  /** @private */
  onReceivedKeyboardFocus_() {
    // FocusOutlineManager relies on the most recent event fired on the
    // document. When the tab strip first gains keyboard focus, no such event
    // exists yet, so the outline needs to be explicitly set to visible.
    this.focusOutlineManager_.visible = true;
    this.$('tabstrip-tab').focus();
  }

  /**
   * @param {number} tabId
   * @private
   */
  onTabActivated_(tabId) {
    if (this.activatingTabId_ === tabId) {
      this.tabStripEmbedderProxy_.reportTabActivationDuration(
          Date.now() - this.activatingTabIdTimestamp_);
    }
    this.activatingTabId_ = undefined;
    this.activatingTabIdTimestamp_ = undefined;

    // There may be more than 1 TabElement marked as active if other events
    // have updated a Tab to have an active state. For example, if a
    // tab is created with an already active state, there may be 2 active
    // TabElements: the newly created tab and the previously active tab.
    this.$all('tabstrip-tab[active]').forEach((previouslyActiveTab) => {
      if (previouslyActiveTab.tab.id !== tabId) {
        previouslyActiveTab.tab = /** @type {!TabData} */ (
            Object.assign({}, previouslyActiveTab.tab, {active: false}));
      }
    });

    const newlyActiveTab = this.findTabElement_(tabId);
    if (newlyActiveTab) {
      newlyActiveTab.tab = /** @type {!TabData} */ (
          Object.assign({}, newlyActiveTab.tab, {active: true}));
      if (!this.tabStripEmbedderProxy_.isVisible()) {
        this.scrollToTab_(newlyActiveTab);
      }
    }
  }

  /**
   * @param {number} id The tab ID
   * @private
   */
  onTabActivating_(id) {
    assert(this.activatingTabId_ === undefined);
    const activeTab = this.getActiveTab_();
    if (activeTab && activeTab.tab.id === id) {
      return;
    }
    this.activatingTabId_ = id;
    this.activatingTabIdTimestamp_ = Date.now();
  }

  /**
   * @param {number} id
   * @private
   */
  onTabCloseCancelled_(id) {
    const tabElement = this.findTabElement_(id);
    if (!tabElement) {
      return;
    }
    tabElement.resetSwipe();
  }

  /** @private */
  onShowContextMenu_() {
    // If we do not have a touch point don't show the context menu.
    if (!this.lastTouchPoint_) {
      return;
    }

    if (this.lastTargetedTab_) {
      const position = getContextMenuPosition(this.lastTargetedTab_);
      this.tabStripEmbedderProxy_.showTabContextMenu(
          this.lastTargetedTab_.tab.id, position.x, position.y);
    } else {
      this.tabStripEmbedderProxy_.showBackgroundContextMenu(
          this.lastTouchPoint_.clientX, this.lastTouchPoint_.clientY);
    }
  }

  /**
   * @param {!TabData} tab
   * @private
   */
  onTabCreated_(tab) {
    const droppedTabElement = this.findTabElement_(tab.id);
    if (droppedTabElement) {
      droppedTabElement.tab = tab;
      droppedTabElement.setDragging(false);
      this.tabsApi_.setThumbnailTracked(tab.id, true);
      return;
    }

    const tabElement = this.createTabElement_(tab);
    this.placeTabElement(tabElement, tab.index, tab.pinned, tab.groupId);
    this.addAnimationPromise_(tabElement.slideIn());
    if (tab.active) {
      this.scrollToTab_(tabElement);
    }
  }

  /**
   * @param {string} groupId
   * @private
   */
  onTabGroupClosed_(groupId) {
    const tabGroupElement = this.findTabGroupElement_(groupId);
    if (!tabGroupElement) {
      return;
    }
    tabGroupElement.remove();
  }

  /**
   * @param {string} groupId
   * @param {number} index
   * @private
   */
  onTabGroupMoved_(groupId, index) {
    const tabGroupElement = this.findTabGroupElement_(groupId);
    if (!tabGroupElement) {
      return;
    }
    this.placeTabGroupElement(tabGroupElement, index);
  }

  /**
   * @param {number} tabId
   * @param {number} index
   * @param {string} groupId
   * @private
   */
  onTabGroupStateChanged_(tabId, index, groupId) {
    const tabElement = this.findTabElement_(tabId);
    tabElement.tab = /** @type {!TabData} */ (
        Object.assign({}, tabElement.tab, {groupId: groupId}));
    this.placeTabElement(tabElement, index, false, groupId);
  }

  /**
   * @param {string} groupId
   * @param {!TabGroupVisualData} visualData
   * @private
   */
  onTabGroupVisualsChanged_(groupId, visualData) {
    const tabGroupElement = this.findTabGroupElement_(groupId);
    tabGroupElement.updateVisuals(visualData);
  }

  /**
   * @param {number} tabId
   * @param {number} newIndex
   * @param {boolean} pinned
   * @private
   */
  onTabMoved_(tabId, newIndex, pinned) {
    const movedTab = this.findTabElement_(tabId);
    if (movedTab) {
      this.placeTabElement(movedTab, newIndex, pinned, movedTab.tab.groupId);
      if (movedTab.tab.active) {
        this.scrollToTab_(movedTab);
      }
    }
  }

  /**
   * @param {number} tabId
   * @private
   */
  onTabRemoved_(tabId) {
    const tabElement = this.findTabElement_(tabId);
    if (tabElement) {
      this.addAnimationPromise_(tabElement.slideOut());
    }
  }

  /**
   * @param {number} oldId
   * @param {number} newId
   * @private
   */
  onTabReplaced_(oldId, newId) {
    const tabElement = this.findTabElement_(oldId);
    if (!tabElement) {
      return;
    }

    tabElement.tab = /** @type {!TabData} */ (
        Object.assign({}, tabElement.tab, {id: newId}));
  }

  /**
   * @param {!TabData} tab
   * @private
   */
  onTabUpdated_(tab) {
    const tabElement = this.findTabElement_(tab.id);
    if (!tabElement) {
      return;
    }

    const previousTab = tabElement.tab;
    tabElement.tab = tab;

    if (previousTab.pinned !== tab.pinned) {
      // If the tab is being pinned or unpinned, we need to move it to its new
      // location
      this.placeTabElement(tabElement, tab.index, tab.pinned, tab.groupId);
      if (tab.active) {
        this.scrollToTab_(tabElement);
      }
      this.updateThumbnailTrackStatus_(tabElement);
    }
  }

  /** @private */
  onWindowBlur_() {
    if (this.shadowRoot.activeElement) {
      // Blur the currently focused element when the window is blurred. This
      // prevents the screen reader from momentarily reading out the
      // previously focused element when the focus returns to this window.
      this.shadowRoot.activeElement.blur();
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onScroll_(e) {
    this.clearScrollTimeout_();
    this.scrollingTimeoutId_ = setTimeout(() => {
      this.flushThumbnailTracker_();
      this.clearScrollTimeout_();
    }, 100);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onTouchStart_(event) {
    const composedPath = /** @type {!Array<!Element>} */ (event.composedPath());
    const dragOverTabElement =
        /** @type {?TabElement} */ (composedPath.find(isTabElement));
    this.lastTargetedTab_ = dragOverTabElement;
    const touch = event.changedTouches[0];
    this.lastTouchPoint_ = {clientX: touch.clientX, clientY: touch.clientY};
  }

  /** @private */
  clearLastTargetedTab_() {
    if (this.lastTargetedTab_) {
      this.lastTargetedTab_.setTouchPressed(false);
    }
    this.lastTargetedTab_ = null;
    this.lastTouchPoint_ = undefined;
  }

  /**
   * @param {!TabElement} element
   * @param {number} index
   * @param {boolean} pinned
   * @param {string=} groupId
   */
  placeTabElement(element, index, pinned, groupId) {
    const isInserting = !element.isConnected;

    const previousIndex = isInserting ? -1 : this.getIndexOfTab(element);
    const previousParent = element.parentElement;
    this.updateTabElementDomPosition_(element, index, pinned, groupId);

    if (!isInserting && previousParent === element.parentElement) {
      // Only animate if the tab is being moved within the same parent. Tab
      // moves that change pinned state or grouped states do not animate.
      animateElementMoved(element, previousIndex, index);
    }

    if (isInserting) {
      this.updateThumbnailTrackStatus_(element);
    }
  }

  /**
   * @param {!TabGroupElement} element
   * @param {number} index
   */
  placeTabGroupElement(element, index) {
    const previousDomIndex =
        Array.from(this.unpinnedTabsElement_.children).indexOf(element);
    if (element.isConnected && element.childElementCount &&
        this.getIndexOfTab(
            /** @type {!TabElement} */ (element.firstElementChild)) < index) {
      // If moving after its original position, the index value needs to be
      // offset by 1 to consider itself already attached to the DOM.
      index++;
    }

    let elementAtIndex = this.$all('tabstrip-tab')[index];
    if (elementAtIndex && elementAtIndex.parentElement &&
        isTabGroupElement(elementAtIndex.parentElement)) {
      elementAtIndex = elementAtIndex.parentElement;
    }

    this.unpinnedTabsElement_.insertBefore(element, elementAtIndex);

    // Animating the TabGroupElement move should be treated the same as
    // animating a TabElement. Therefore, treat indices as if they were mere
    // tabs and do not use the group's model index as they are not as accurate
    // in representing DOM movements.
    animateElementMoved(
        element, previousDomIndex,
        Array.from(this.unpinnedTabsElement_.children).indexOf(element));
  }

  /** @private */
  flushThumbnailTracker_() {
    this.thumbnailTracker_.forEach((shouldTrack, tabId) => {
      this.tabsApi_.setThumbnailTracked(tabId, shouldTrack);
    });
    this.thumbnailTracker_.clear();
  }

  /** @private */
  scrollToActiveTab_() {
    const activeTab = this.getActiveTab_();
    if (!activeTab) {
      return;
    }

    this.scrollToTab_(activeTab);
  }

  /**
   * @param {!TabElement} tabElement
   * @private
   */
  scrollToTab_(tabElement) {
    const tabElementWidth = this.getLayoutVariable_(LayoutVariable.TAB_WIDTH);
    const tabElementRect = tabElement.getBoundingClientRect();
    // In RTL languages, the TabElement's scale animation scales from right to
    // left. Therefore, the value of its getBoundingClientRect().left may not be
    // accurate of its final rendered size because the element may not have
    // fully scaled to the left yet.
    const tabElementLeft =
        isRTL() ? tabElementRect.right - tabElementWidth : tabElementRect.left;

    const newTabButtonSpace =
        this.getLayoutVariable_(LayoutVariable.NEW_TAB_BUTTON_WIDTH) +
        this.getLayoutVariable_(LayoutVariable.NEW_TAB_BUTTON_MARGIN);
    const leftBoundary =
        isRTL() ? SCROLL_PADDING + newTabButtonSpace : SCROLL_PADDING;

    let scrollBy = 0;
    if (tabElementLeft === leftBoundary) {
      // Perfectly aligned to the left.
      return;
    } else if (tabElementLeft < leftBoundary) {
      // If the element's left is to the left of the left boundary, scroll
      // such that the element's left edge is aligned with the left boundary.
      scrollBy = tabElementLeft - leftBoundary;
    } else {
      const tabElementRight = tabElementLeft + tabElementWidth;
      const rightBoundary = isRTL() ?
          this.getLayoutVariable_(LayoutVariable.VIEWPORT_WIDTH) -
              SCROLL_PADDING :
          this.getLayoutVariable_(LayoutVariable.VIEWPORT_WIDTH) -
              SCROLL_PADDING - newTabButtonSpace;

      if (tabElementRight > rightBoundary) {
        scrollBy = (tabElementRight) - rightBoundary;
      } else {
        // Perfectly aligned to the right.
        return;
      }
    }

    this.animateScrollPosition_(scrollBy);
  }

  /** @return {boolean} */
  shouldPreventDrag() {
    return this.$all('tabstrip-tab').length === 1;
  }

  /**
   * @param {number} tabId
   * @param {string} imgData
   * @private
   */
  tabThumbnailUpdated_(tabId, imgData) {
    const tab = this.findTabElement_(tabId);
    if (tab) {
      tab.updateThumbnail(imgData);
    }
  }

  /**
   * @param {!TabElement} element
   * @param {number} index
   * @param {boolean} pinned
   * @param {string=} groupId
   * @private
   */
  updateTabElementDomPosition_(element, index, pinned, groupId) {
    // Remove the element if it already exists in the DOM. This simplifies
    // the way indices work as it does not have to count its old index in
    // the initial layout of the DOM.
    element.remove();

    if (pinned) {
      this.pinnedTabsElement_.insertBefore(
          element, this.pinnedTabsElement_.childNodes[index]);
    } else {
      let elementToInsert = element;
      let elementAtIndex = this.$all('tabstrip-tab').item(index);
      let parentElement = this.unpinnedTabsElement_;

      if (groupId) {
        let tabGroupElement = this.findTabGroupElement_(groupId);
        if (tabGroupElement) {
          // If a TabGroupElement already exists, add the TabElement to it.
          parentElement = tabGroupElement;
        } else {
          // If a TabGroupElement does not exist, create one and add the
          // TabGroupElement into the DOM.
          tabGroupElement = document.createElement('tabstrip-tab-group');
          tabGroupElement.setAttribute('data-group-id', groupId);
          tabGroupElement.appendChild(element);
          elementToInsert = tabGroupElement;
        }
      }

      if (elementAtIndex && elementAtIndex.parentElement &&
          isTabGroupElement(elementAtIndex.parentElement) &&
          (elementAtIndex.previousElementSibling === null &&
           elementAtIndex.tab.groupId !== groupId)) {
        // If the element at the model index is in a group, and the group is
        // different from the new tab's group, and is the first element in its
        // group, insert the new element before its TabGroupElement. If a
        // TabElement is being sandwiched between two TabElements in a group, it
        // can be assumed that the tab will eventually be inserted into the
        // group as well.
        elementAtIndex = elementAtIndex.parentElement;
      }

      if (elementAtIndex && elementAtIndex.parentElement === parentElement) {
        parentElement.insertBefore(elementToInsert, elementAtIndex);
      } else {
        parentElement.appendChild(elementToInsert);
      }
    }
  }

  /**
   * @param {!TabElement} tabElement
   * @private
   */
  updateThumbnailTrackStatus_(tabElement) {
    if (!tabElement.tab) {
      return;
    }

    if (this.tabStripEmbedderProxy_.isVisible() && !tabElement.tab.pinned) {
      // If the tab strip is visible and the tab is not pinned, let the
      // IntersectionObserver start observing the TabElement to automatically
      // determine if the tab's thumbnail should be tracked.
      this.intersectionObserver_.observe(tabElement);
    } else {
      // If the tab strip is not visible or the tab is pinned, the tab does not
      // need to show or update any thumbnails.
      this.intersectionObserver_.unobserve(tabElement);
      this.tabsApi_.setThumbnailTracked(tabElement.tab.id, false);
    }
  }
}

customElements.define('tabstrip-tab-list', TabListElement);
