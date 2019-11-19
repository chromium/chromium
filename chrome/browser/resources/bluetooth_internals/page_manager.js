// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui.pageManager', function() {
  /**
   * PageManager contains a list of root Page and overlay Page objects and
   * handles "navigation" by showing and hiding these pages and overlays. On
   * initial load, PageManager can use the path to open the correct hierarchy
   * of pages and overlay(s). Handlers for user events, like pressing buttons,
   * can call into PageManager to open a particular overlay or cancel an
   * existing overlay.
   */
  const PageManager = {
    /**
     * True if page is served from a dialog.
     * @type {boolean}
     */
    isDialog: false,

    /**
     * Offset of page container in pixels. Uber pages that use the side menu
     * can override this with the setter.
     * @type {number}
     */
    horizontalOffset_: 23,

    /**
     * Root pages. Maps lower-case page names to the respective page object.
     * @type {!Object<!cr.ui.pageManager.Page>}
     */
    registeredPages: {},

    /**
     * Pages which are meant to behave like modal dialogs. Maps lower-case
     * overlay names to the respective overlay object.
     * @type {!Object<!cr.ui.pageManager.Page>}
     * @private
     */
    registeredOverlayPages: {},

    /**
     * Observers will be notified when opening and closing overlays.
     * @type {!Array<!cr.ui.pageManager.PageManager.Observer>}
     */
    observers_: [],

    /**
     * Initializes the complete page.
     * @param {cr.ui.pageManager.Page} defaultPage The page to be shown when no
     *     page is specified in the path.
     */
    initialize: function(defaultPage) {
      this.defaultPage_ = defaultPage;

      cr.ui.FocusOutlineManager.forDocument(document);
      document.addEventListener('scroll', this.handleScroll_.bind(this));

      // Trigger the scroll handler manually to set the initial state.
      this.handleScroll_();

      // Shake the dialog if the user clicks outside the dialog bounds.
      const containers = /** @type {!NodeList<!HTMLElement>} */ (
          document.querySelectorAll('body > .overlay'));
      for (let i = 0; i < containers.length; i++) {
        const overlay = containers[i];
        cr.ui.overlay.setupOverlay(overlay);
        overlay.addEventListener(
            'cancelOverlay', this.cancelOverlay.bind(this));
      }

      cr.ui.overlay.globalInitialization();
    },

    /**
     * Registers new page.
     * @param {!cr.ui.pageManager.Page} page Page to register.
     */
    register: function(page) {
      this.registeredPages[page.name.toLowerCase()] = page;
      page.initializePage();
    },

    /**
     * Unregisters an existing page.
     * @param {!cr.ui.pageManager.Page} page Page to unregister.
     */
    unregister: function(page) {
      delete this.registeredPages[page.name.toLowerCase()];
    },

    /**
     * Registers a new Overlay page.
     * @param {!cr.ui.pageManager.Page} overlay Overlay to register.
     * @param {cr.ui.pageManager.Page} parentPage Associated parent page for
     *     this overlay.
     * @param {Array} associatedControls Array of control elements associated
     *     with this page.
     */
    registerOverlay: function(overlay, parentPage, associatedControls) {
      this.registeredOverlayPages[overlay.name.toLowerCase()] = overlay;
      overlay.parentPage = parentPage;
      if (associatedControls) {
        overlay.associatedControls = associatedControls;
        if (associatedControls.length) {
          overlay.associatedSection =
              this.findSectionForNode_(associatedControls[0]);
        }

        // Sanity check.
        for (let i = 0; i < associatedControls.length; ++i) {
          assert(associatedControls[i], 'Invalid element passed.');
        }
      }

      overlay.tab = undefined;
      overlay.isOverlay = true;

      overlay.reverseButtonStrip();
      overlay.initializePage();
    },

    /**
     * Shows the default page.
     * @param {boolean=} opt_updateHistory If we should update the history after
     *     showing the page (defaults to true).
     */
    showDefaultPage: function(opt_updateHistory) {
      assert(
          this.defaultPage_ instanceof cr.ui.pageManager.Page,
          'PageManager must be initialized with a default page.');
      this.showPageByName(this.defaultPage_.name, opt_updateHistory);
    },

    /**
     * Shows a registered page. This handles both root and overlay pages.
     * @param {string} pageName Page name.
     * @param {boolean=} opt_updateHistory If we should update the history after
     *     showing the page (defaults to true).
     * @param {Object=} opt_propertyBag An optional bag of properties including
     *     replaceState (if history state should be replaced instead of pushed).
     *     hash (a hash state to attach to the page).
     */
    showPageByName: function(pageName, opt_updateHistory, opt_propertyBag) {
      opt_updateHistory = opt_updateHistory !== false;
      opt_propertyBag = opt_propertyBag || {};

      // If a bubble is currently being shown, hide it.
      this.hideBubble();

      // Find the currently visible root-level page.
      let rootPage = null;
      for (const name in this.registeredPages) {
        const page = this.registeredPages[name];
        if (page.visible && !page.parentPage) {
          rootPage = page;
          break;
        }
      }

      // Find the target page.
      let targetPage = this.registeredPages[pageName.toLowerCase()];
      if (!targetPage || !targetPage.canShowPage()) {
        // If it's not a page, try it as an overlay.
        const hash = opt_propertyBag.hash || '';
        if (!targetPage && this.showOverlay_(pageName, hash, rootPage)) {
          if (opt_updateHistory) {
            this.updateHistoryState_(!!opt_propertyBag.replaceState);
          }
          this.updateTitle_();
          return;
        }
        targetPage = this.defaultPage_;
      }

      pageName = targetPage.name.toLowerCase();
      const targetPageWasVisible = targetPage.visible;

      // Determine if the root page is 'sticky', meaning that it
      // shouldn't change when showing an overlay. This can happen for special
      // pages like Search.
      const isRootPageLocked =
          rootPage && rootPage.sticky && targetPage.parentPage;

      // Notify pages if they will be hidden.
      this.forEachPage_(!isRootPageLocked, function(page) {
        if (page.name != pageName && !this.isAncestorOfPage(page, targetPage)) {
          page.willHidePage();
        }
      });

      // Update the page's hash.
      targetPage.hash = opt_propertyBag.hash || '';

      // Update visibilities to show only the hierarchy of the target page.
      this.forEachPage_(!isRootPageLocked, function(page) {
        page.visible =
            page.name == pageName || this.isAncestorOfPage(page, targetPage);
      });

      // Update the history and current location.
      if (opt_updateHistory) {
        this.updateHistoryState_(!!opt_propertyBag.replaceState);
      }

      // Update focus if any other control was focused on the previous page,
      // or the previous page is not known.
      if (document.activeElement != document.body &&
          (!rootPage || rootPage.pageDiv.contains(document.activeElement))) {
        targetPage.focus();
      }

      // Notify pages if they were shown.
      this.forEachPage_(!isRootPageLocked, function(page) {
        if (!targetPageWasVisible &&
            (page.name == pageName ||
             this.isAncestorOfPage(page, targetPage))) {
          page.didShowPage();
        }
      });

      // If the target page was already visible, notify it that its hash
      // changed externally.
      if (targetPageWasVisible) {
        targetPage.didChangeHash();
      }

      // Update the document title. Do this after didShowPage was called, in
      // case a page decides to change its title.
      this.updateTitle_();
    },

    /**
     * Returns the name of the page from the current path.
     * @return {string} Name of the page specified by the current path.
     */
    getPageNameFromPath: function() {
      const path = location.pathname;
      if (path.length <= 1) {
        return this.defaultPage_.name;
      }

      // Skip starting slash and remove trailing slash (if any).
      return path.slice(1).replace(/\/$/, '');
    },

    /**
     * Gets the level of the page. Root pages (e.g., BrowserOptions) are at
     * level 0.
     * @return {number} How far down this page is from the root page.
     */
    getNestingLevel: function(page) {
      let level = 0;
      let parent = page.parentPage;
      while (parent) {
        level++;
        parent = parent.parentPage;
      }
      return level;
    },

    /**
     * Checks whether one page is an ancestor of the other page in terms of
     * subpage nesting.
     * @param {cr.ui.pageManager.Page} potentialAncestor Potential ancestor.
     * @param {cr.ui.pageManager.Page} potentialDescendent Potential descendent.
     * @return {boolean} True if |potentialDescendent| is nested under
     *     |potentialAncestor|.
     */
    isAncestorOfPage: function(potentialAncestor, potentialDescendent) {
      let parent = potentialDescendent.parentPage;
      while (parent) {
        if (parent == potentialAncestor) {
          return true;
        }
        parent = parent.parentPage;
      }
      return false;
    },

    /**
     * Returns true if the page is a direct descendent of a root page, or if
     * the page is considered always on top. Doesn't consider visibility.
     * @param {cr.ui.pageManager.Page} page Page to check.
     * @return {boolean} True if |page| is a top-level overlay.
     */
    isTopLevelOverlay: function(page) {
      return page.isOverlay &&
          (page.alwaysOnTop || this.getNestingLevel(page) == 1);
    },

    /**
     * Called when an page is shown or hidden to update the root page
     * based on the page's new visibility.
     * @param {cr.ui.pageManager.Page} page The page being made visible or
     *     invisible.
     */
    onPageVisibilityChanged: function(page) {
      this.updateRootPageFreezeState();

      for (let i = 0; i < this.observers_.length; ++i) {
        this.observers_[i].onPageVisibilityChanged(page);
      }

      if (!page.visible && this.isTopLevelOverlay(page)) {
        this.updateScrollPosition_();
      }
    },

    /**
     * Called when a page's hash changes. If the page is the topmost visible
     * page, the history state is updated.
     * @param {cr.ui.pageManager.Page} page The page whose hash has changed.
     */
    onPageHashChanged: function(page) {
      if (page == this.getTopmostVisiblePage()) {
        this.updateHistoryState_(false);
      }
    },

    /**
     * Returns the topmost visible page, or null if no page is visible.
     * @return {cr.ui.pageManager.Page} The topmost visible page.
     */
    getTopmostVisiblePage: function() {
      // Check overlays first since they're top-most if visible.
      return this.getVisibleOverlay_() ||
          this.getTopmostVisibleNonOverlayPage_();
    },

    /**
     * Closes the visible overlay. Updates the history state after closing the
     * overlay.
     */
    closeOverlay: function() {
      const overlay = this.getVisibleOverlay_();
      if (!overlay) {
        return;
      }

      overlay.visible = false;
      overlay.didClosePage();

      this.updateHistoryState_(false);
      this.updateTitle_();

      this.restoreLastFocusedElement_();
    },

    /**
     * Closes all overlays and updates the history after each closed overlay.
     */
    closeAllOverlays: function() {
      while (this.isOverlayVisible_()) {
        this.closeOverlay();
      }
    },

    /**
     * Cancels (closes) the overlay, due to the user pressing <Esc>.
     */
    cancelOverlay: function() {
      // Blur the active element to ensure any changed pref value is saved.
      document.activeElement.blur();
      const overlay = this.getVisibleOverlay_();
      if (!overlay) {
        return;
      }
      // Let the overlay handle the <Esc> if it wants to.
      if (overlay.handleCancel) {
        overlay.handleCancel();
        this.restoreLastFocusedElement_();
      } else {
        this.closeOverlay();
      }
    },

    /**
     * Shows an informational bubble displaying |content| and pointing at the
     * |target| element. If |content| has focusable elements, they join the
     * current page's tab order as siblings of |domSibling|.
     * @param {HTMLDivElement} content The content of the bubble.
     * @param {HTMLElement} target The element at which the bubble points.
     * @param {HTMLElement} domSibling The element after which the bubble is
     *     added to the DOM.
     * @param {cr.ui.ArrowLocation} location The arrow location.
     */
    showBubble: function(content, target, domSibling, location) {
      this.hideBubble();

      const bubble = new cr.ui.AutoCloseBubble;
      bubble.anchorNode = target;
      bubble.domSibling = domSibling;
      bubble.arrowLocation = location;
      bubble.content = content;
      bubble.show();
      this.bubble_ = bubble;
    },

    /**
     * Hides the currently visible bubble, if any.
     */
    hideBubble: function() {
      if (this.bubble_) {
        this.bubble_.hide();
      }
    },

    /**
     * Returns the currently visible bubble, or null if no bubble is visible.
     * @return {cr.ui.AutoCloseBubble} The bubble currently being shown.
     */
    getVisibleBubble: function() {
      const bubble = this.bubble_;
      return bubble && !bubble.hidden ? bubble : null;
    },

    /**
     * Callback for window.onpopstate to handle back/forward navigations.
     * @param {string} pageName The current page name.
     * @param {string} hash The hash to pass into the page.
     * @param {Object} data State data pushed into history.
     */
    setState: function(pageName, hash, data) {
      const currentOverlay = this.getVisibleOverlay_();
      const lowercaseName = pageName.toLowerCase();
      const newPage = this.registeredPages[lowercaseName] ||
          this.registeredOverlayPages[lowercaseName] || this.defaultPage_;
      if (currentOverlay && !this.isAncestorOfPage(currentOverlay, newPage)) {
        currentOverlay.visible = false;
        currentOverlay.didClosePage();
      }
      this.showPageByName(pageName, false, {hash: hash});
    },


    /**
     * Whether the page is still loading (i.e. onload hasn't finished running).
     * @return {boolean} Whether the page is still loading.
     */
    isLoading: function() {
      return document.documentElement.classList.contains('loading');
    },

    /**
     * Callback for window.onbeforeunload. Used to notify overlays that they
     * will be closed.
     */
    willClose: function() {
      const overlay = this.getVisibleOverlay_();
      if (overlay) {
        overlay.didClosePage();
      }
    },

    /**
     * Freezes/unfreezes the scroll position of the root page based on the
     * current page stack.
     */
    updateRootPageFreezeState: function() {
      const topPage = this.getTopmostVisiblePage();
      if (topPage) {
        this.setRootPageFrozen_(topPage.isOverlay);
      }
    },

    /**
     * Change the horizontal offset used to reposition elements while showing an
     * overlay from the default.
     */
    set horizontalOffset(value) {
      this.horizontalOffset_ = value;
    },

    /**
     * @param {!cr.ui.pageManager.PageManager.Observer} observer The observer to
     *     register.
     */
    addObserver: function(observer) {
      this.observers_.push(observer);
    },

    /**
     * Shows a registered overlay page. Does not update history.
     * @param {string} overlayName Page name.
     * @param {string} hash The hash state to associate with the overlay.
     * @param {cr.ui.pageManager.Page} rootPage The currently visible root-level
     *     page.
     * @return {boolean} Whether we showed an overlay.
     * @private
     */
    showOverlay_: function(overlayName, hash, rootPage) {
      const overlay = this.registeredOverlayPages[overlayName.toLowerCase()];
      if (!overlay || !overlay.canShowPage()) {
        return false;
      }

      const focusOutlineManager =
          cr.ui.FocusOutlineManager.forDocument(document);

      // Save the currently focused element in the page for restoration later.
      const currentPage = this.getTopmostVisiblePage();
      if (currentPage && focusOutlineManager.visible) {
        currentPage.lastFocusedElement = document.activeElement;
      }

      if ((!rootPage || !rootPage.sticky) && overlay.parentPage &&
          !overlay.parentPage.visible) {
        this.showPageByName(overlay.parentPage.name, false);
      }

      overlay.hash = hash;
      if (!overlay.visible) {
        overlay.visible = true;
        overlay.didShowPage();
      } else {
        overlay.didChangeHash();
      }

      if (focusOutlineManager.visible) {
        overlay.focus();
      }

      if (!overlay.pageDiv.contains(document.activeElement)) {
        document.activeElement.blur();
      }

      if ($('search-field') && $('search-field').value == '') {
        const section = overlay.associatedSection;
        if (section) {
          /** @suppress {checkTypes|checkVars} */
          (function() {
            options.BrowserOptions.scrollToSection(section);
          })();
        }
      }

      return true;
    },

    /**
     * Returns whether or not an overlay is visible.
     * @return {boolean} True if an overlay is visible.
     * @private
     */
    isOverlayVisible_: function() {
      return this.getVisibleOverlay_() != null;
    },

    /**
     * Returns the currently visible overlay, or null if no page is visible.
     * @return {cr.ui.pageManager.Page} The visible overlay.
     * @private
     */
    getVisibleOverlay_: function() {
      let topmostPage = null;
      for (const name in this.registeredOverlayPages) {
        const page = this.registeredOverlayPages[name];
        if (!page.visible) {
          continue;
        }

        if (page.alwaysOnTop) {
          return page;
        }

        if (!topmostPage ||
            this.getNestingLevel(page) > this.getNestingLevel(topmostPage)) {
          topmostPage = page;
        }
      }
      return topmostPage;
    },

    /**
     * Returns the topmost visible page (overlays excluded).
     * @return {cr.ui.pageManager.Page} The topmost visible page aside from any
     *     overlays.
     * @private
     */
    getTopmostVisibleNonOverlayPage_: function() {
      for (const name in this.registeredPages) {
        const page = this.registeredPages[name];
        if (page.visible) {
          return page;
        }
      }

      return null;
    },

    /**
     * Scrolls the page to the correct position (the top when opening an
     * overlay, or the old scroll position a previously hidden overlay
     * becomes visible).
     * @private
     */
    updateScrollPosition_: function() {
      const container = $('page-container');
      const scrollTop = container.oldScrollTop || 0;
      container.oldScrollTop = undefined;
      window.scroll(scrollLeftForDocument(document), scrollTop);
    },

    /**
     * Updates the title to the title of the current page, or of the topmost
     * visible page with a non-empty title.
     * @private
     */
    updateTitle_: function() {
      let page = this.getTopmostVisiblePage();
      while (page) {
        if (page.title) {
          for (let i = 0; i < this.observers_.length; ++i) {
            this.observers_[i].updateTitle(page.title);
          }
          return;
        }
        page = page.parentPage;
      }
    },

    /**
     * Constructs a new path to push onto the history stack, using observers
     * to update the history.
     * @param {boolean} replace If true, handlers should replace the current
     *     history event rather than create new ones.
     * @private
     */
    updateHistoryState_: function(replace) {
      if (this.isDialog) {
        return;
      }

      const page = this.getTopmostVisiblePage();
      let path = window.location.pathname + window.location.hash;
      if (path) {
        // Remove trailing slash.
        path = path.slice(1).replace(/\/(?:#|$)/, '');
      }

      // If the page is already in history (the user may have clicked the same
      // link twice, or this is the initial load), do nothing.
      const newPath = (page == this.defaultPage_ ? '' : page.name) + page.hash;
      if (path == newPath) {
        return;
      }

      for (let i = 0; i < this.observers_.length; ++i) {
        this.observers_[i].updateHistory(newPath, replace);
      }
    },

    /**
     * Restores the last focused element on a given page.
     * @private
     */
    restoreLastFocusedElement_: function() {
      const currentPage = this.getTopmostVisiblePage();

      if (!currentPage.lastFocusedElement) {
        return;
      }

      if (cr.ui.FocusOutlineManager.forDocument(document).visible) {
        currentPage.lastFocusedElement.focus();
      }

      currentPage.lastFocusedElement = null;
    },

    /**
     * Find an enclosing section for an element if it exists.
     * @param {Node} node Element to search.
     * @return {Node} The section element, or null.
     * @private
     */
    findSectionForNode_: function(node) {
      while (node = node.parentNode) {
        if (node.nodeName == 'SECTION') {
          return node;
        }
      }
      return null;
    },

    /**
     * Freezes/unfreezes the scroll position of the root page container.
     * @param {boolean} freeze Whether the page should be frozen.
     * @private
     */
    setRootPageFrozen_: function(freeze) {
      const container = $('page-container');
      if (container.classList.contains('frozen') == freeze) {
        return;
      }

      if (freeze) {
        // Lock the width, since auto width computation may change.
        container.style.width = window.getComputedStyle(container).width;
        container.oldScrollTop = scrollTopForDocument(document);
        container.classList.add('frozen');
        const verticalPosition =
            container.getBoundingClientRect().top - container.oldScrollTop;
        container.style.top = verticalPosition + 'px';
        this.updateFrozenElementHorizontalPosition_(container);
      } else {
        container.classList.remove('frozen');
        container.style.top = '';
        container.style.left = '';
        container.style.right = '';
        container.style.width = '';
      }
    },

    /**
     * Called when the page is scrolled; moves elements that are position:fixed
     * but should only behave as if they are fixed for vertical scrolling.
     * @private
     */
    handleScroll_: function() {
      this.updateAllFrozenElementPositions_();
    },

    /**
     * Updates all frozen pages to match the horizontal scroll position.
     * @private
     */
    updateAllFrozenElementPositions_: function() {
      const frozenElements = document.querySelectorAll('.frozen');
      for (let i = 0; i < frozenElements.length; i++) {
        this.updateFrozenElementHorizontalPosition_(frozenElements[i]);
      }
    },

    /**
     * Updates the given frozen element to match the horizontal scroll position.
     * @param {HTMLElement} e The frozen element to update.
     * @private
     */
    updateFrozenElementHorizontalPosition_: function(e) {
      if (isRTL()) {
        e.style.right = this.horizontalOffset + 'px';
      } else {
        const scrollLeft = scrollLeftForDocument(document);
        e.style.left = this.horizontalOffset - scrollLeft + 'px';
      }
    },

    /**
     * Calls the given callback with each registered page.
     * @param {boolean} includeRootPages Whether the callback should be called
     *     for the root pages.
     * @param {function(cr.ui.pageManager.Page)} callback The callback.
     * @private
     */
    forEachPage_: function(includeRootPages, callback) {
      let pageNames = Object.keys(this.registeredOverlayPages);
      if (includeRootPages) {
        pageNames = Object.keys(this.registeredPages).concat(pageNames);
      }

      pageNames.forEach(function(name) {
        callback.call(
            this,
            this.registeredOverlayPages[name] || this.registeredPages[name]);
      }, this);
    },
  };

  /**
   * An observer of PageManager.
   * @constructor
   */
  PageManager.Observer = function() {};

  PageManager.Observer.prototype = {
    /**
     * Called when a page is being shown or has been hidden.
     * @param {cr.ui.pageManager.Page} page The page being shown or hidden.
     */
    onPageVisibilityChanged: function(page) {},

    /**
     * Called when a new title should be set.
     * @param {string} title The title to set.
     */
    updateTitle: function(title) {},

    /**
     * Called when a page is navigated to.
     * @param {string} path The path of the page being visited.
     * @param {boolean} replace If true, allow no history events to be created.
     */
    updateHistory: function(path, replace) {},
  };

  // Export
  return {PageManager: PageManager};
});
