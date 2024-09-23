// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './tab.js';
import './tab_group.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {isRTL} from 'chrome://resources/js/util.js';

import type {DragManagerDelegate} from './drag_manager.js';
import {DragManager} from './drag_manager.js';
import {isTabElement, TabElement} from './tab.js';
import type {TabGroupElement} from './tab_group.js';
import {isDragHandle, isTabGroupElement} from './tab_group.js';
import {getTemplate} from './tab_list.html.js';
import type {Tab, TabGroupVisualData} from './tab_strip.mojom-webui.js';
import type {TabsApiProxy} from './tabs_api_proxy.js';
import {TabsApiProxyImpl} from './tabs_api_proxy.js';

/**
 * The amount of padding to leave between the edge of the screen and the active
 * tab when auto-scrolling. This should leave some room to show the previous or
 * next tab to afford to users that there more tabs if the user scrolls.
 */
const SCROLL_PADDING: number = 32;

let scrollAnimationEnabled: boolean = true;

const TOUCH_CONTEXT_MENU_OFFSET_X: number = 8;

const TOUCH_CONTEXT_MENU_OFFSET_Y: number = -40;

/**
 * Context menu should position below the element for touch.
 */
function getContextMenuPosition(element: Element): {x: number, y: number} {
  const rect = element.getBoundingClientRect();
  return {
    x: rect.left + TOUCH_CONTEXT_MENU_OFFSET_X,
    y: rect.bottom + TOUCH_CONTEXT_MENU_OFFSET_Y,
  };
}

export function setScrollAnimationEnabledForTesting(enabled: boolean) {
  scrollAnimationEnabled = enabled;
}

enum LayoutVariable {
  VIEWPORT_WIDTH = '--tabstrip-viewport-width',
  TAB_WIDTH = '--tabstrip-tab-thumbnail-width',
}

/**
 * Animates a series of elements to indicate that tabs have moved position.
 */
function animateElementMoved(
    movedElement: Element, prevIndex: number, newIndex: number) {
  // Direction is -1 for moving towards a lower index, +1 for moving
  // towards a higher index. If moving towards a lower index, the TabList needs
  // to animate everything from the movedElement's current index to its prev
  // index by traversing the nextElementSibling of each element because the
  // movedElement is now at a preceding position from all the elements it has
  // slid across. If moving towards a higher index, the TabList needs to
  // traverse the previousElementSiblings.
  const direction = Math.sign(newIndex - prevIndex);

  function getSiblingToAnimate(element: Element): Element|null {
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
 */
function slideElement(element: Element, prevIndex: number, newIndex: number) {
  let horizontalMovement = newIndex - prevIndex;
  let verticalMovement = 0;

  if (isTabElement(element) && (element as TabElement).tab.pinned) {
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

  (element as TabElement | TabGroupElement).isValidDragOverTarget = false;
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
    (element as TabElement | TabGroupElement).isValidDragOverTarget = true;
  }
  animation.oncancel = onComplete;
  animation.onfinish = onComplete;
}

export class TabListElement extends CustomElement implements
    DragManagerDelegate {
  animationPromises: Promise<void>;
  private currentScrollUpdateFrame_: number|null;
  private draggedItem_?: TabElement|TabGroupElement;
  private dropPlaceholder_: HTMLElement;
  private focusOutlineManager_: FocusOutlineManager;
  private thumbnailTracker_: Map<number, boolean>;
  private intersectionObserver_: IntersectionObserver;

  private activatingTabId_?: number;
  private activatingTabIdTimestamp_?: number;  // In ms.
  private eventTracker_: EventTracker;
  private lastTargetedItem_: TabElement|TabGroupElement|null = null;
  private lastTouchPoint_?: {clientX: number, clientY: number};
  private pinnedTabsElement_: Element;
  private tabsApi_: TabsApiProxy;
  private unpinnedTabsElement_: Element;
  private scrollingTimeoutId_: number;

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();

    /**
     * A chain of promises that the tab list needs to keep track of. The chain
     * is useful in cases when the list needs to wait for all animations to
     * finish in order to get accurate pixels (such as getting the position of a
     * tab) or accurate element counts.
     */
    this.animationPromises = Promise.resolve();

    /**
     * The ID of the current animation frame that is in queue to update the
     * scroll position.
     */
    this.currentScrollUpdateFrame_ = null;

    /**
     * The element that is currently being dragged.
     */
    this.draggedItem_;

    this.dropPlaceholder_ = document.createElement('div');
    this.dropPlaceholder_.id = 'dropPlaceholder';

    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);

    /**
     * Map of tab IDs to whether or not the tab's thumbnail should be tracked.
     */
    this.thumbnailTracker_ = new Map();

    /**
     * An intersection observer is needed to observe which TabElements are
     * currently in view or close to being in view, which will help determine
     * which thumbnails need to be tracked to stay fresh and which can be
     * untracked until they become visible.
     */
    this.intersectionObserver_ = new IntersectionObserver(entries => {
      for (const entry of entries) {
        this.thumbnailTracker_.set(
            (entry.target as TabElement).tab.id, entry.isIntersecting);
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

    this.eventTracker_ = new EventTracker();

    this.pinnedTabsElement_ = this.getRequiredElement('#pinnedTabs');

    this.tabsApi_ = TabsApiProxyImpl.getInstance();

    this.unpinnedTabsElement_ = this.getRequiredElement('#unpinnedTabs');

    /**
     * Timeout that is created at every scroll event and is either canceled at
     * each subsequent scroll event or resolves after a few milliseconds after
     * the last scroll event.
     */
    this.scrollingTimeoutId_ = -1;

    const callbackRouter = this.tabsApi_.getCallbackRouter();
    callbackRouter.layoutChanged.addListener(
        this.applyCssDictionary_.bind(this));

    callbackRouter.tabThumbnailUpdated.addListener(
        this.tabThumbnailUpdated_.bind(this));

    callbackRouter.longPress.addListener(() => this.handleLongPress_());

    callbackRouter.contextMenuClosed.addListener(
        () => this.clearLastTargetedItem_());

    callbackRouter.receivedKeyboardFocus.addListener(
        () => this.onReceivedKeyboardFocus_());

    callbackRouter.themeChanged.addListener(() => {
      // Refetch theme group color and tab favicons on theme change.
      this.fetchAndUpdateGroupData_();
      this.fetchAndUpdateTabs_();
    });

    this.eventTracker_.add(
        document, 'contextmenu', (e: Event) => this.onContextMenu_(e));
    this.eventTracker_.add(
        document, 'pointerup',
        (e: Event) => this.onPointerUp_(e as PointerEvent));
    this.eventTracker_.add(
        document, 'visibilitychange', () => this.onDocumentVisibilityChange_());
    this.eventTracker_.add(window, 'blur', () => this.onWindowBlur_());
    this.eventTracker_.add(this, 'scroll', (e: Event) => this.onScroll_(e));
    this.eventTracker_.add(
        document, 'touchstart',
        (e: Event) => this.onTouchStart_(e as TouchEvent));
    // Touchmove events happen when a user has started a touch gesture sequence
    // and proceeded to move their touch pointer across the screen. Ensure that
    // we clear the `last_targeted_item_` in these cases to ensure the pressed
    // visual is cleared away.
    this.eventTracker_.add(
        document, 'touchmove', () => this.clearLastTargetedItem_());

    const dragManager = new DragManager(this);
    dragManager.startObserving();

    ColorChangeUpdater.forDocument().start();
  }

  private addAnimationPromise_(promise: Promise<void>) {
    this.animationPromises = this.animationPromises.then(() => promise);
  }

  private animateScrollPosition_(scrollBy: number) {
    if (this.currentScrollUpdateFrame_) {
      cancelAnimationFrame(this.currentScrollUpdateFrame_);
      this.currentScrollUpdateFrame_ = null;
    }

    const prevScrollLeft = this.scrollLeft;
    if (!scrollAnimationEnabled || !this.tabsApi_.isVisible()) {
      // Do not animate if tab strip is not visible.
      this.scrollLeft = prevScrollLeft + scrollBy;
      return;
    }

    const duration = 350;
    let startTime: number;

    const onAnimationFrame = (currentTime: number) => {
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

  private applyCssDictionary_(dictionary: {[key: string]: string}) {
    for (const [cssVariable, value] of Object.entries(dictionary)) {
      this.style.setProperty(cssVariable, value);
    }
  }

  private clearScrollTimeout_() {
    clearTimeout(this.scrollingTimeoutId_);
    this.scrollingTimeoutId_ = -1;
  }

  connectedCallback() {
    this.tabsApi_.getLayout().then(
        ({layout}) => this.applyCssDictionary_(layout));

    const getTabsStartTimestamp = Date.now();
    this.tabsApi_.getTabs().then(({tabs}) => {
      this.tabsApi_.reportTabDataReceivedDuration(
          tabs.length, Date.now() - getTabsStartTimestamp);

      const createTabsStartTimestamp = Date.now();
      tabs.forEach(tab => this.onTabCreated_(tab));
      this.fetchAndUpdateGroupData_();
      this.tabsApi_.reportTabCreationDuration(
          tabs.length, Date.now() - createTabsStartTimestamp);

      const callbackRouter = this.tabsApi_.getCallbackRouter();
      callbackRouter.showContextMenu.addListener(
          () => this.onShowContextMenu_());
      callbackRouter.tabCreated.addListener(this.onTabCreated_.bind(this));
      callbackRouter.tabMoved.addListener(this.onTabMoved_.bind(this));
      callbackRouter.tabRemoved.addListener(this.onTabRemoved_.bind(this));
      callbackRouter.tabReplaced.addListener(this.onTabReplaced_.bind(this));
      callbackRouter.tabUpdated.addListener(this.onTabUpdated_.bind(this));
      callbackRouter.tabActiveChanged.addListener(
          this.onTabActivated_.bind(this));
      callbackRouter.tabCloseCancelled.addListener(
          this.onTabCloseCancelled_.bind(this));
      callbackRouter.tabGroupStateChanged.addListener(
          this.onTabGroupStateChanged_.bind(this));
      callbackRouter.tabGroupClosed.addListener(
          this.onTabGroupClosed_.bind(this));
      callbackRouter.tabGroupMoved.addListener(
          this.onTabGroupMoved_.bind(this));
      callbackRouter.tabGroupVisualsChanged.addListener(
          this.onTabGroupVisualsChanged_.bind(this));
    });
  }

  disconnectedCallback() {
    this.eventTracker_.removeAll();
  }

  private createTabElement_(tab: Tab): TabElement {
    const tabElement = new TabElement();
    tabElement.tab = tab;
    tabElement.onTabActivating = (id) => {
      this.onTabActivating_(id);
    };
    return tabElement;
  }

  private findTabElement_(tabId: number): TabElement|null {
    return this.$<TabElement>(`tabstrip-tab[data-tab-id="${tabId}"]`);
  }

  private findTabGroupElement_(groupId: string): TabGroupElement|null {
    return this.$<TabGroupElement>(
        `tabstrip-tab-group[data-group-id="${groupId}"]`);
  }

  private fetchAndUpdateGroupData_() {
    const tabGroupElements = this.$all('tabstrip-tab-group');
    this.tabsApi_.getGroupVisualData().then(({data}) => {
      tabGroupElements.forEach(tabGroupElement => {
        const visualData = data[tabGroupElement.dataset['groupId']!];
        assert(visualData);
        tabGroupElement.updateVisuals(visualData);
      });
    });
  }

  private fetchAndUpdateTabs_() {
    this.tabsApi_.getTabs().then(({tabs}) => {
      tabs.forEach(tab => this.onTabUpdated_(tab));
    });
  }

  private getActiveTab_(): TabElement|null {
    return this.$<TabElement>('tabstrip-tab[active]');
  }

  getIndexOfTab(tabElement: TabElement): number {
    return Array.prototype.indexOf.call(this.$all('tabstrip-tab'), tabElement);
  }

  /** @return in pixels */
  private getLayoutVariable_(variable: LayoutVariable): number {
    return parseInt(this.style.getPropertyValue(variable), 10);
  }

  private handleLongPress_() {
    if (this.lastTargetedItem_) {
      this.lastTargetedItem_.setTouchPressed(true);
    }
  }

  private onContextMenu_(event: Event) {
    // Prevent the default context menu from triggering.
    event.preventDefault();
  }

  private onPointerUp_(event: PointerEvent) {
    event.stopPropagation();
    if (event.pointerType !== 'touch' && event.button === 2) {
      // If processing an uncaught right click event show the background context
      // menu.
      this.tabsApi_.showBackgroundContextMenu(event.clientX, event.clientY);
    }
  }

  private onDocumentVisibilityChange_() {
    if (!this.tabsApi_.isVisible()) {
      this.scrollToActiveTab_();
    }

    this.unpinnedTabsElement_.childNodes.forEach(element => {
      if (isTabGroupElement(element as Element)) {
        element.childNodes.forEach(
            tabElement =>
                this.updateThumbnailTrackStatus_(tabElement as TabElement));
      } else {
        this.updateThumbnailTrackStatus_(element as TabElement);
      }
    });
  }

  private onReceivedKeyboardFocus_() {
    // FocusOutlineManager relies on the most recent event fired on the
    // document. When the tab strip first gains keyboard focus, no such event
    // exists yet, so the outline needs to be explicitly set to visible.
    this.focusOutlineManager_.visible = true;
    this.$<TabElement>('tabstrip-tab')!.focus();
  }

  private updatePreviouslyActiveTabs_(activeTabId: number) {
    // There may be more than 1 TabElement marked as active if other events
    // have updated a Tab to have an active state. For example, if a
    // tab is created with an already active state, there may be 2 active
    // TabElements: the newly created tab and the previously active tab.
    this.$all<TabElement>('tabstrip-tab[active]')
        .forEach((previouslyActiveTab) => {
          if (previouslyActiveTab.tab.id !== activeTabId) {
            previouslyActiveTab.tab = /** @type {!Tab} */ (
                Object.assign({}, previouslyActiveTab.tab, {active: false}));
          }
        });
  }

  private onTabActivated_(tabId: number) {
    if (this.activatingTabId_ === tabId) {
      this.tabsApi_.reportTabActivationDuration(
          Date.now() - this.activatingTabIdTimestamp_!);
    }
    this.activatingTabId_ = undefined;
    this.activatingTabIdTimestamp_ = undefined;

    this.updatePreviouslyActiveTabs_(tabId);
    const newlyActiveTab = this.findTabElement_(tabId);
    if (newlyActiveTab) {
      newlyActiveTab.tab =
          Object.assign({}, newlyActiveTab.tab, {active: true});
      if (!this.tabsApi_.isVisible()) {
        this.scrollToTab_(newlyActiveTab);
      }
    }
  }

  private onTabActivating_(id: number) {
    // onTabActivating_() is called when the user clicks on a tab in JavaScript.
    // We then expect a callback asynchronously from the browser after the tab
    // we clicked on has finally activated. We may incur multiple calls to
    // onTabActivating_()  before the active tab actually changes so we only
    // consider the most recent activating action when recording metrics. (See
    // crbug.com/1333405)
    const activeTab = this.getActiveTab_();
    if (activeTab && activeTab.tab.id === id) {
      return;
    }
    this.activatingTabId_ = id;
    this.activatingTabIdTimestamp_ = Date.now();
  }

  private onTabCloseCancelled_(id: number) {
    const tabElement = this.findTabElement_(id);
    if (!tabElement) {
      return;
    }
    tabElement.resetSwipe();
  }

  private onShowContextMenu_() {
    // If we do not have a touch point don't show the context menu.
    if (!this.lastTouchPoint_) {
      return;
    }

    if (this.lastTargetedItem_ && isTabElement(this.lastTargetedItem_)) {
      const position = getContextMenuPosition(this.lastTargetedItem_);
      this.tabsApi_.showTabContextMenu(
          (this.lastTargetedItem_ as TabElement).tab.id, position.x,
          position.y);
    } else {
      this.tabsApi_.showBackgroundContextMenu(
          this.lastTouchPoint_.clientX, this.lastTouchPoint_.clientY);
    }
  }

  private onTabCreated_(tab: Tab) {
    const droppedTabElement = this.findTabElement_(tab.id);
    if (droppedTabElement) {
      droppedTabElement.tab = tab;
      droppedTabElement.setDragging(false);
      this.tabsApi_.setThumbnailTracked(tab.id, true);
      return;
    }

    const tabElement = this.createTabElement_(tab);
    this.placeTabElement(tabElement, tab.index, tab.pinned, tab.groupId);
    if (tab.active) {
      this.updatePreviouslyActiveTabs_(tab.id);
      this.scrollToTab_(tabElement);
    }
  }

  private onTabGroupClosed_(groupId: string) {
    const tabGroupElement = this.findTabGroupElement_(groupId);
    if (!tabGroupElement) {
      return;
    }
    tabGroupElement.remove();
  }

  private onTabGroupMoved_(groupId: string, index: number) {
    const tabGroupElement = this.findTabGroupElement_(groupId);
    if (!tabGroupElement) {
      return;
    }
    this.placeTabGroupElement(tabGroupElement, index);
  }

  private onTabGroupStateChanged_(
      tabId: number, index: number, groupId: string) {
    const tabElement = this.findTabElement_(tabId)!;
    tabElement.tab = Object.assign({}, tabElement.tab, {groupId: groupId});
    this.placeTabElement(tabElement, index, false, groupId);
  }

  private onTabGroupVisualsChanged_(
      groupId: string, visualData: TabGroupVisualData) {
    const tabGroupElement = this.findTabGroupElement_(groupId)!;
    tabGroupElement.updateVisuals(visualData);
  }

  private onTabMoved_(tabId: number, newIndex: number, pinned: boolean) {
    const movedTab = this.findTabElement_(tabId);
    if (movedTab) {
      this.placeTabElement(movedTab, newIndex, pinned, movedTab.tab.groupId);
      if (movedTab.tab.active) {
        this.scrollToTab_(movedTab);
      }
    }
  }

  private onTabRemoved_(tabId: number) {
    const tabElement = this.findTabElement_(tabId);
    if (tabElement) {
      this.addAnimationPromise_(tabElement.slideOut());
    }
  }

  private onTabReplaced_(oldId: number, newId: number) {
    const tabElement = this.findTabElement_(oldId);
    if (!tabElement) {
      return;
    }

    tabElement.tab = Object.assign({}, tabElement.tab, {id: newId});
  }

  private onTabUpdated_(tab: Tab) {
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

  private onWindowBlur_() {
    if (this.shadowRoot!.activeElement) {
      // Blur the currently focused element when the window is blurred. This
      // prevents the screen reader from momentarily reading out the
      // previously focused element when the focus returns to this window.
      (this.shadowRoot!.activeElement as HTMLElement).blur();
    }
  }

  private onScroll_(_e: Event) {
    this.clearScrollTimeout_();
    this.scrollingTimeoutId_ = setTimeout(() => {
      this.flushThumbnailTracker_();
      this.clearScrollTimeout_();
    }, 100);
  }

  private onTouchStart_(event: TouchEvent) {
    const composedPath = event.composedPath() as Element[];
    const dragOverTabElement =
        (composedPath.find(isTabElement) ||
         composedPath.find(isTabGroupElement) || null) as TabElement |
        TabGroupElement | null;

    // Make sure drag handle is under touch point when dragging a tab group.
    if (dragOverTabElement && isTabGroupElement(dragOverTabElement) &&
        !composedPath.find(isDragHandle)) {
      return;
    }

    this.lastTargetedItem_ = dragOverTabElement;
    const touch = event.changedTouches[0]!;
    this.lastTouchPoint_ = {clientX: touch.clientX, clientY: touch.clientY};
  }

  private clearLastTargetedItem_() {
    if (this.lastTargetedItem_) {
      this.lastTargetedItem_.setTouchPressed(false);
    }
    this.lastTargetedItem_ = null;
    this.lastTouchPoint_ = undefined;
  }

  placeTabElement(
      element: TabElement, index: number, pinned: boolean,
      groupId: string|null) {
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

  placeTabGroupElement(element: TabGroupElement, index: number) {
    const previousDomIndex =
        Array.from(this.unpinnedTabsElement_.children).indexOf(element);
    if (element.isConnected && element.childElementCount &&
        this.getIndexOfTab(element.firstElementChild as TabElement) < index) {
      // If moving after its original position, the index value needs to be
      // offset by 1 to consider itself already attached to the DOM.
      index++;
    }

    let elementAtIndex: TabGroupElement|TabElement =
        this.$all('tabstrip-tab')[index]!;
    if (elementAtIndex && elementAtIndex.parentElement &&
        isTabGroupElement(elementAtIndex.parentElement)) {
      elementAtIndex = elementAtIndex.parentElement as TabGroupElement;
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

  private flushThumbnailTracker_() {
    this.thumbnailTracker_.forEach((shouldTrack, tabId) => {
      this.tabsApi_.setThumbnailTracked(tabId, shouldTrack);
    });
    this.thumbnailTracker_.clear();
  }

  private scrollToActiveTab_() {
    const activeTab = this.getActiveTab_();
    if (!activeTab) {
      return;
    }

    this.scrollToTab_(activeTab);
  }

  private scrollToTab_(tabElement: TabElement) {
    const tabElementWidth = this.getLayoutVariable_(LayoutVariable.TAB_WIDTH);
    const tabElementRect = tabElement.getBoundingClientRect();
    // In RTL languages, the TabElement's scale animation scales from right to
    // left. Therefore, the value of its getBoundingClientRect().left may not be
    // accurate of its final rendered size because the element may not have
    // fully scaled to the left yet.
    const tabElementLeft =
        isRTL() ? tabElementRect.right - tabElementWidth : tabElementRect.left;
    const leftBoundary = SCROLL_PADDING;

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
      const rightBoundary =
          this.getLayoutVariable_(LayoutVariable.VIEWPORT_WIDTH) -
          SCROLL_PADDING;
      if (tabElementRight > rightBoundary) {
        scrollBy = (tabElementRight) - rightBoundary;
      } else {
        // Perfectly aligned to the right.
        return;
      }
    }

    this.animateScrollPosition_(scrollBy);
  }

  shouldPreventDrag(isDraggingTab: boolean): boolean {
    if (isDraggingTab) {
      // Do not allow dragging a tab if there's only 1 tab.
      return this.$all('tabstrip-tab').length === 1;
    } else {
      // Do not allow dragging the tab group with no others outside of the tab
      // group. In this case there is only 1 pinned and unpinned top level
      // element, which is the dragging tab group itself.
      return (this.pinnedTabsElement_.childElementCount +
              this.unpinnedTabsElement_.childElementCount) === 1;
    }
  }

  private tabThumbnailUpdated_(tabId: number, imgData: string) {
    const tab = this.findTabElement_(tabId);
    if (tab) {
      tab.updateThumbnail(imgData);
    }
  }

  private updateTabElementDomPosition_(
      element: TabElement, index: number, pinned: boolean,
      groupId: string|null) {
    // Remove the element if it already exists in the DOM. This simplifies
    // the way indices work as it does not have to count its old index in
    // the initial layout of the DOM.
    element.remove();

    if (pinned) {
      this.pinnedTabsElement_.insertBefore(
          element, this.pinnedTabsElement_.childNodes[index]!);
    } else {
      let elementToInsert: TabElement|TabGroupElement = element;
      let elementAtIndex: TabElement|TabGroupElement =
          this.$all<TabElement>('tabstrip-tab').item(index);
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
        elementAtIndex = elementAtIndex.parentElement as TabGroupElement;
      }

      if (elementAtIndex && elementAtIndex.parentElement === parentElement) {
        parentElement.insertBefore(elementToInsert, elementAtIndex);
      } else {
        parentElement.appendChild(elementToInsert);
      }
    }
  }

  private updateThumbnailTrackStatus_(tabElement: TabElement) {
    if (!tabElement.hasTabModel()) {
      return;
    }

    if (this.tabsApi_.isVisible() && !tabElement.tab.pinned) {
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

declare global {
  interface HTMLElementTagNameMap {
    'tabstrip-tab-list': TabListElement;
  }
}

customElements.define('tabstrip-tab-list', TabListElement);
