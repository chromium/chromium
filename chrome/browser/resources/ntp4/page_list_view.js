// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PageListView implementation.
 * PageListView manages page list, dot list, switcher buttons and handles apps
 * pages callbacks from backend.
 *
 * Note that you need to have AppLauncherHandler in your WebUI to use this code.
 */

/**
 * @typedef {{app_launch_ordinal: string,
 *            description: string,
 *            detailsUrl: string,
 *            direction: string,
 *            enabled: boolean,
 *            full_name: string,
 *            full_name_direction: string,
 *            homepageUrl: string,
 *            icon_big: string,
 *            icon_big_exists: boolean,
 *            icon_small: string,
 *            icon_small_exists: boolean,
 *            id: string,
 *            is_component: boolean,
 *            is_webstore: boolean,
 *            isLocallyInstalled: boolean,
 *            kioskEnabled: boolean,
 *            kioskMode: boolean,
 *            kioskOnly: boolean,
 *            launch_container: number,
 *            launch_type: number,
 *            mayChangeLaunchType: boolean,
 *            mayCreateShortcuts: boolean,
 *            mayDisable: boolean,
 *            name: string,
 *            offlineEnabled: boolean,
 *            optionsUrl: string,
 *            packagedApp: boolean,
 *            page_index: number,
 *            title: string,
 *            url: string,
 *            version: string}}
 * @see chrome/browser/ui/webui/ntp/app_launcher_handler.cc
 */
let AppInfo;

cr.define('ntp', function() {
  'use strict';

  /**
   * Creates a PageListView object.
   * @constructor
   * @extends {Object}
   */
  function PageListView() {}

  PageListView.prototype = {
    /**
     * The CardSlider object to use for changing app pages.
     * @type {cr.ui.CardSlider|undefined}
     */
    cardSlider: undefined,

    /**
     * The frame div for this.cardSlider.
     * @type {!Element|undefined}
     */
    sliderFrame: undefined,

    /**
     * The 'page-list' element.
     * @type {!Element|undefined}
     */
    pageList: undefined,

    /**
     * A list of all 'tile-page' elements.
     * @type {!HTMLCollection<!ntp.TilePage>|undefined}
     */
    tilePages: undefined,

    /**
     * A list of all 'apps-page' elements.
     * @type {!HTMLCollection<!ntp.AppsPage>|undefined}
     */
    appsPages: undefined,

    /**
     * The 'dots-list' element.
     * @type {!ntp.DotList|undefined}
     */
    dotList: undefined,

    /**
     * The left and right paging buttons.
     * @type {!ntp.PageSwitcher|undefined}
     */
    pageSwitcherStart: undefined,
    pageSwitcherEnd: undefined,

    /**
     * The 'trash' element.  Note that technically this is unnecessary,
     * JavaScript creates the object for us based on the id.  But I don't want
     * to rely on the ID being the same, and JSCompiler doesn't know about it.
     * @type {!Element|undefined}
     */
    trash: undefined,

    /**
     * The index of the page that is currently shown. For example if the third
     * page is showing, this will be 2.
     * @type {number}
     */
    shownPageIndex: 0,

    /**
     * EventTracker for managing event listeners for page events.
     * @type {!EventTracker}
     */
    eventTracker: new EventTracker,

    /**
     * If non-null, this is the ID of the app to highlight to the user the next
     * time getAppsCallback runs. "Highlight" in this case means to switch to
     * the page and run the new tile animation.
     * @type {?string}
     */
    highlightAppId: null,

    /**
     * Initializes page list view.
     * @param {!Element} pageList A DIV element to host all pages.
     * @param {!Element} dotList An UL element to host nav dots. Each dot
     *     represents a page.
     * @param {!Element} cardSliderFrame The card slider frame that hosts
     *     pageList and switcher buttons.
     * @param {!Element|undefined} opt_trash Optional trash element.
     * @param {!ntp.PageSwitcher|undefined} opt_pageSwitcherStart Optional start
     *     page switcher button.
     * @param {!ntp.PageSwitcher|undefined} opt_pageSwitcherEnd Optional end
     *     page switcher button.
     */
    initialize: function(
        pageList, dotList, cardSliderFrame, opt_trash, opt_pageSwitcherStart,
        opt_pageSwitcherEnd) {
      this.pageList = pageList;

      this.dotList = /** @type {!ntp.DotList} */ (dotList);
      cr.ui.decorate(this.dotList, ntp.DotList);

      this.trash = opt_trash;
      if (this.trash) {
        new ntp.Trash(this.trash);
      }

      this.pageSwitcherStart = opt_pageSwitcherStart;
      if (this.pageSwitcherStart) {
        ntp.initializePageSwitcher(this.pageSwitcherStart);
      }

      this.pageSwitcherEnd = opt_pageSwitcherEnd;
      if (this.pageSwitcherEnd) {
        ntp.initializePageSwitcher(this.pageSwitcherEnd);
      }

      this.shownPageIndex = loadTimeData.getInteger('shown_page_index');

      // Request data on the apps so we can fill them in.
      // Note that this is kicked off asynchronously.  'getAppsCallback' will
      // be invoked at some point after this function returns.
      chrome.send('getApps');

      document.addEventListener('keydown', this.onDocKeyDown_.bind(this));

      this.tilePages = /** @type {!HTMLCollection<!ntp.TilePage>} */ (
          this.pageList.getElementsByClassName('tile-page'));
      this.appsPages = /** @type {!HTMLCollection<!ntp.AppsPage>} */ (
          this.pageList.getElementsByClassName('apps-page'));

      // Initialize the cardSlider without any cards at the moment.
      this.sliderFrame = cardSliderFrame;
      this.cardSlider = new cr.ui.CardSlider(
          this.sliderFrame, this.pageList, this.sliderFrame.offsetWidth);

      // Prevent touch events from triggering any sort of native scrolling if
      // there are multiple cards in the slider frame.
      const cardSlider = this.cardSlider;
      cardSliderFrame.addEventListener('touchmove', function(e) {
        if (cardSlider.cardCount <= 1) {
          return;
        }
        e.preventDefault();
      }, true);

      // Handle mousewheel events anywhere in the card slider, so that wheel
      // events on the page switchers will still scroll the page.
      // This listener must be added before the card slider is initialized,
      // because it needs to be called before the card slider's handler.
      cardSliderFrame.addEventListener('mousewheel', function(e) {
        if (/** @type {!ntp.TilePage} */ (cardSlider.currentCardValue)
                .handleMouseWheel(e)) {
          e.preventDefault();            // Prevent default scroll behavior.
          e.stopImmediatePropagation();  // Prevent horizontal card flipping.
        }
      });

      this.cardSlider.initialize(
          loadTimeData.getBoolean('isSwipeTrackingFromScrollEventsEnabled'));

      // Handle events from the card slider.
      this.pageList.addEventListener(
          'cardSlider:card_changed', this.onCardChanged_.bind(this));
      this.pageList.addEventListener(
          'cardSlider:card_added', this.onCardAdded_.bind(this));
      this.pageList.addEventListener(
          'cardSlider:card_removed', this.onCardRemoved_.bind(this));

      // Ensure the slider is resized appropriately with the window.
      window.addEventListener('resize', this.onWindowResize_.bind(this));

      // Update apps when online state changes.
      window.addEventListener(
          'online', this.updateOfflineEnabledApps_.bind(this));
      window.addEventListener(
          'offline', this.updateOfflineEnabledApps_.bind(this));
    },

    /**
     * Appends a tile page.
     *
     * @param {!ntp.TilePage} page The page element.
     * @param {string} title The title of the tile page.
     * @param {boolean} titleIsEditable If true, the title can be changed.
     * @param {ntp.TilePage=} opt_refNode Optional reference node to insert in
     *     front of.
     * When opt_refNode is falsey, |page| will just be appended to the end of
     * the page list.
     */
    appendTilePage: function(page, title, titleIsEditable, opt_refNode) {
      if (opt_refNode) {
        const refIndex = this.getTilePageIndex(opt_refNode);
        this.cardSlider.addCardAtIndex(page, refIndex);
      } else {
        this.cardSlider.appendCard(page);
      }

      // If we're appending an AppsPage and it's a temporary page, animate it.
      const animate =
          page instanceof ntp.AppsPage && page.classList.contains('temporary');
      // Make a deep copy of the dot template to add a new one.
      const newDot = new ntp.NavDot(page, title, titleIsEditable, animate);
      page.navigationDot = newDot;
      this.dotList.insertBefore(
          newDot, opt_refNode ? opt_refNode.navigationDot : null);
      // Set a tab index on the first dot.
      if (this.dotList.dots.length == 1) {
        newDot.tabIndex = 3;
      }

      this.eventTracker.add(page, 'pagelayout', this.onPageLayout_.bind(this));
    },

    /**
     * Called by chrome when an app has changed positions.
     * @param {AppInfo} appData The data for the app. This contains page and
     *     position indices.
     */
    appMoved: function(appData) {
      const app = /** @type {ntp.App} */ ($(appData.id));
      assert(app, 'trying to move an app that doesn\'t exist');
      app.remove(false);

      this.appsPages[appData.page_index].insertApp(appData, false);
    },

    /**
     * Called by chrome when an existing app has been disabled or
     * removed/uninstalled from chrome.
     * @param {AppInfo} appData A data structure full of relevant information
     *     for the app.
     * @param {boolean} isUninstall True if the app is being uninstalled;
     *     false if the app is being disabled.
     * @param {boolean} fromPage True if the removal was from the current page.
     */
    appRemoved: function(appData, isUninstall, fromPage) {
      const app = /** @type {ntp.App} */ ($(appData.id));
      assert(app, 'trying to remove an app that doesn\'t exist');

      if (!isUninstall) {
        app.replaceAppData(appData);
      } else {
        app.remove(!!fromPage);
      }
    },

    /**
     * @return {boolean} If the page is still starting up.
     * @private
     */
    isStartingUp_: function() {
      return document.documentElement.classList.contains('starting-up');
    },

    /**
     * Tracks whether apps have been loaded at least once.
     * @type {boolean}
     * @private
     */
    appsLoaded_: false,

    /**
     * Callback invoked by chrome with the apps available.
     *
     * Note that calls to this function can occur at any time, not just in
     * response to a getApps request. For example, when a user
     * installs/uninstalls an app on another synchronized devices.
     * @param {{apps: Array<AppInfo>, appPageNames: Array<string>}} data
     *     An object with all the data on available applications.
     */
    getAppsCallback: function(data) {
      const startTime = Date.now();

      // Remember this to select the correct card when done rebuilding.
      const prevCurrentCard = this.cardSlider.currentCard;

      // Make removal of pages and dots as quick as possible with less DOM
      // operations, reflows, or repaints. We set currentCard = 0 and remove
      // from the end to not encounter any auto-magic card selections in the
      // process and we hide the card slider throughout.
      this.cardSlider.currentCard = 0;

      // Clear any existing apps pages and dots.
      // TODO(rbyers): It might be nice to preserve animation of dots after an
      // uninstall. Could we re-use the existing page and dot elements?  It
      // seems unfortunate to have Chrome send us the entire apps list after an
      // uninstall.
      while (this.appsPages.length > 0) {
        this.removeTilePageAndDot_(this.appsPages[this.appsPages.length - 1]);
      }

      // Get the array of apps and add any special synthesized entries
      const apps = data.apps;

      // Get a list of page names
      const pageNames = data.appPageNames;

      function stringListIsEmpty(list) {
        for (let i = 0; i < list.length; i++) {
          if (list[i]) {
            return false;
          }
        }
        return true;
      }

      // Sort by launch ordinal
      apps.sort(function(a, b) {
        return a.app_launch_ordinal > b.app_launch_ordinal ?
            1 :
            a.app_launch_ordinal < b.app_launch_ordinal ? -1 : 0;
      });

      // An app to animate (in case it was just installed).
      let highlightApp;

      // If there are any pages after the apps, add new pages before them.
      const lastAppsPage = (this.appsPages.length > 0) ?
          this.appsPages[this.appsPages.length - 1] :
          null;
      const lastAppsPageIndex = (lastAppsPage != null) ?
          Array.prototype.indexOf.call(this.tilePages, lastAppsPage) :
          -1;
      const nextPageAfterApps = lastAppsPageIndex != -1 ?
          this.tilePages[lastAppsPageIndex + 1] :
          null;

      // Add the apps, creating pages as necessary
      for (let i = 0; i < apps.length; i++) {
        const app = apps[i];
        const pageIndex = app.page_index || 0;
        while (pageIndex >= this.appsPages.length) {
          let pageName = loadTimeData.getString('appDefaultPageName');
          if (this.appsPages.length < pageNames.length) {
            pageName = pageNames[this.appsPages.length];
          }

          const origPageCount = this.appsPages.length;
          this.appendTilePage(
              new ntp.AppsPage(), pageName, true, nextPageAfterApps);
          // Confirm that appsPages is a live object, updated when a new page is
          // added (otherwise we'd have an infinite loop)
          assert(
              this.appsPages.length == origPageCount + 1, 'expected new page');
        }

        if (app.id == this.highlightAppId) {
          highlightApp = app;
        } else {
          this.appsPages[pageIndex].insertApp(app, false);
        }
      }

      this.cardSlider.currentCard = prevCurrentCard;

      if (highlightApp) {
        this.appAdded(highlightApp, true);
      }

      logEvent('apps.layout: ' + (Date.now() - startTime));

      // Tell the slider about the pages and mark the current page.
      this.updateSliderCards();
      this.cardSlider.currentCardValue.navigationDot.classList.add('selected');

      if (!this.appsLoaded_) {
        this.appsLoaded_ = true;
        cr.dispatchSimpleEvent(document, 'sectionready', true, true);
      }
    },

    /**
     * Called by chrome when a new app has been added to chrome or has been
     * enabled if previously disabled.
     * @param {AppInfo} appData A data structure full of relevant information
     *     for the app.
     * @param {boolean=} opt_highlight Whether the app about to be added should
     *     be highlighted.
     */
    appAdded: function(appData, opt_highlight) {
      if (appData.id == this.highlightAppId) {
        opt_highlight = true;
        this.highlightAppId = null;
      }

      const pageIndex = appData.page_index || 0;

      if (pageIndex >= this.appsPages.length) {
        while (pageIndex >= this.appsPages.length) {
          this.appendTilePage(
              new ntp.AppsPage(), loadTimeData.getString('appDefaultPageName'),
              true);
        }
        this.updateSliderCards();
      }

      const page = this.appsPages[pageIndex];
      const app = /** @type {?ntp.App} */ ($(appData.id));
      if (app) {
        app.replaceAppData(appData);
      } else if (opt_highlight) {
        page.insertAndHighlightApp(appData);
        this.setShownPage_(appData.page_index);
      } else {
        page.insertApp(appData, false);
      }
    },

    /**
     * Callback invoked by chrome whenever an app preference changes.
     * @param {Object} data An object with all the data on available
     *     applications.
     */
    appsPrefChangedCallback: function(data) {
      for (let i = 0; i < data.apps.length; ++i) {
        $(data.apps[i].id).appData = data.apps[i];
      }

      // Set the App dot names.
      const dots = this.dotList.getElementsByClassName('dot');
      for (let i = 0; i < dots.length; ++i) {
        dots[i].displayTitle = data.appPageNames[i] || '';
      }
    },

    /**
     * Invoked whenever the pages in apps-page-list have changed so that
     * the Slider knows about the new elements.
     */
    updateSliderCards: function() {
      const pageNo = Math.max(
          0, Math.min(this.cardSlider.currentCard, this.tilePages.length - 1));
      this.cardSlider.setCards(
          Array.prototype.slice.call(this.tilePages), pageNo);
      this.cardSlider.selectCardByValue(this.appsPages[Math.min(
          this.shownPageIndex, this.appsPages.length - 1)]);
    },

    /**
     * Called whenever tiles should be re-arranging themselves out of the way
     * of a moving or insert tile.
     */
    enterRearrangeMode: function() {
      const tempPage = new ntp.AppsPage();
      tempPage.classList.add('temporary');
      const pageName = loadTimeData.getString('appDefaultPageName');
      this.appendTilePage(tempPage, pageName, true);

      if (ntp.getCurrentlyDraggingTile().firstChild.canBeRemoved()) {
        $('footer').classList.add('showing-trash-mode');
        $('footer-menu-container').style.minWidth = $('trash').offsetWidth -
            $('chrome-web-store-link').offsetWidth + 'px';
      }

      document.documentElement.classList.add('dragging-mode');
    },

    /**
     * Invoked whenever some app is released
     */
    leaveRearrangeMode: function() {
      const tempPage = /** @type {ntp.AppsPage} */ (
          document.querySelector('.tile-page.temporary'));
      if (tempPage) {
        const dot = tempPage.navigationDot;
        if (!tempPage.tileCount &&
            tempPage != this.cardSlider.currentCardValue) {
          this.removeTilePageAndDot_(tempPage, true);
        } else {
          tempPage.classList.remove('temporary');
          this.saveAppPageName(
              tempPage, loadTimeData.getString('appDefaultPageName'));
        }
      }

      $('footer').classList.remove('showing-trash-mode');
      $('footer-menu-container').style.minWidth = '';
      document.documentElement.classList.remove('dragging-mode');
    },

    /**
     * Callback for the 'pagelayout' event.
     * @param {Event} e The event.
     */
    onPageLayout_: function(e) {
      if (Array.prototype.indexOf.call(this.tilePages, e.currentTarget) !=
          this.cardSlider.currentCard) {
        return;
      }

      this.updatePageSwitchers();
    },

    /**
     * Adjusts the size and position of the page switchers according to the
     * layout of the current card, and updates the aria-label attributes of
     * the page switchers.
     */
    updatePageSwitchers: function() {
      if (!this.pageSwitcherStart || !this.pageSwitcherEnd) {
        return;
      }

      const page =
          /** @type {?ntp.TilePage} */ (this.cardSlider.currentCardValue);

      this.pageSwitcherStart.hidden =
          !page || (this.cardSlider.currentCard == 0);
      this.pageSwitcherEnd.hidden = !page ||
          (this.cardSlider.currentCard == this.cardSlider.cardCount - 1);

      if (!page) {
        return;
      }

      const pageSwitcherLeft =
          isRTL() ? this.pageSwitcherEnd : this.pageSwitcherStart;
      const pageSwitcherRight =
          isRTL() ? this.pageSwitcherStart : this.pageSwitcherEnd;
      const scrollbarWidth = page.scrollbarWidth;
      pageSwitcherLeft.style.width = (page.sideMargin + 13) + 'px';
      pageSwitcherLeft.style.left = '0';
      pageSwitcherRight.style.width =
          (page.sideMargin - scrollbarWidth + 13) + 'px';
      pageSwitcherRight.style.right = scrollbarWidth + 'px';

      const offsetTop =
          page.querySelector('.tile-page-content').offsetTop + 'px';
      pageSwitcherLeft.style.top = offsetTop;
      pageSwitcherRight.style.top = offsetTop;
      pageSwitcherLeft.style.paddingBottom = offsetTop;
      pageSwitcherRight.style.paddingBottom = offsetTop;

      // Update the aria-label attributes of the two page switchers.
      this.pageSwitcherStart.updateButtonAccessibleLabel(this.dotList.dots);
      this.pageSwitcherEnd.updateButtonAccessibleLabel(this.dotList.dots);
    },

    /**
     * Returns the index of the given apps page.
     * @param {ntp.AppsPage} page The AppsPage we wish to find.
     * @return {number} The index of |page| or -1 if it is not in the
     *    collection.
     */
    getAppsPageIndex: function(page) {
      return Array.prototype.indexOf.call(this.appsPages, page);
    },

    /**
     * Handler for cardSlider:card_changed events from this.cardSlider.
     * @param {Event} e The cardSlider:card_changed event.
     * @private
     */
    onCardChanged_: function(e) {
      const page = e.cardSlider.currentCardValue;

      // Don't change shownPage until startup is done (and page changes actually
      // reflect user actions).
      if (!this.isStartingUp_()) {
        // TODO(dbeam): is this ever false?
        if (page.classList.contains('apps-page')) {
          this.setShownPage_(this.getAppsPageIndex(page));
        } else {
          console.error('unknown page selected');
        }
      }

      // Update the active dot
      const curDot = this.dotList.getElementsByClassName('selected')[0];
      if (curDot) {
        curDot.classList.remove('selected');
      }
      page.navigationDot.classList.add('selected');
      this.updatePageSwitchers();
    },

    /**
     * Saves/updates the newly selected page to open when first loading the NTP.
     * @param {number} shownPageIndex The new shown page index.
     * @private
     */
    setShownPage_: function(shownPageIndex) {
      assert(shownPageIndex >= 0);
      this.shownPageIndex = shownPageIndex;
      chrome.send('pageSelected', [this.shownPageIndex]);
    },

    /**
     * Listen for card additions to update the page switchers or the current
     * card accordingly.
     * @param {Event} e A card removed or added event.
     */
    onCardAdded_: function(e) {
      // When the second arg passed to insertBefore is falsey, it acts just like
      // appendChild.
      this.pageList.insertBefore(e.addedCard, this.tilePages[e.addedIndex]);
      this.onCardAddedOrRemoved_();
    },

    /**
     * Listen for card removals to update the page switchers or the current card
     * accordingly.
     * @param {Event} e A card removed or added event.
     */
    onCardRemoved_: function(e) {
      e.removedCard.parentNode.removeChild(e.removedCard);
      this.onCardAddedOrRemoved_();
    },

    /**
     * Called when a card is removed or added.
     * @private
     */
    onCardAddedOrRemoved_: function() {
      if (this.isStartingUp_()) {
        return;
      }

      // Without repositioning there were issues - http://crbug.com/133457.
      this.cardSlider.repositionFrame();
      this.updatePageSwitchers();
    },

    /**
     * Save the name of an apps page.
     * Store the apps page name into the preferences store.
     * @param {ntp.AppsPage} appPage The app page for which we wish to save.
     * @param {string} name The name of the page.
     */
    saveAppPageName: function(appPage, name) {
      const index = this.getAppsPageIndex(appPage);
      assert(index != -1);
      chrome.send('saveAppPageName', [name, index]);
    },

    /**
     * Window resize handler.
     * @private
     */
    onWindowResize_: function(e) {
      this.cardSlider.resize(this.sliderFrame.offsetWidth);
      this.updatePageSwitchers();
    },

    /**
     * Listener for offline status change events. Updates apps that are
     * not offline-enabled to be grayscale if the browser is offline.
     * @private
     */
    updateOfflineEnabledApps_: function() {
      const apps = /** @type {!NodeList<!ntp.App>} */ (
          document.querySelectorAll('.app'));
      for (let i = 0; i < apps.length; ++i) {
        if (apps[i].appData.enabled && !apps[i].appData.offlineEnabled) {
          apps[i].setIcon();
          apps[i].loadIcon();
        }
      }
    },

    /**
     * Handler for key events on the page. Ctrl-Arrow will switch the visible
     * page.
     * @param {Event} e The KeyboardEvent.
     * @private
     */
    onDocKeyDown_: function(e) {
      if (!e.ctrlKey || e.altKey || e.metaKey || e.shiftKey) {
        return;
      }

      let direction = 0;
      if (e.key == 'ArrowLeft') {
        direction = -1;
      } else if (e.key == 'ArrowRight') {
        direction = 1;
      } else {
        return;
      }

      const cardIndex = (this.cardSlider.currentCard + direction +
                         this.cardSlider.cardCount) %
          this.cardSlider.cardCount;
      this.cardSlider.selectCard(cardIndex, true);

      e.stopPropagation();
    },

    /**
     * Returns the index of a given tile page.
     * @param {ntp.TilePage} page The TilePage we wish to find.
     * @return {number} The index of |page| or -1 if it is not in the
     *    collection.
     */
    getTilePageIndex: function(page) {
      return Array.prototype.indexOf.call(this.tilePages, page);
    },

    /**
     * Removes a page and navigation dot (if the navdot exists).
     * @param {ntp.TilePage} page The page to be removed.
     * @param {boolean=} opt_animate If the removal should be animated.
     */
    removeTilePageAndDot_: function(page, opt_animate) {
      if (page.navigationDot) {
        page.navigationDot.remove(opt_animate);
      }
      this.cardSlider.removeCard(page);
    },
  };

  return {PageListView: PageListView};
});
