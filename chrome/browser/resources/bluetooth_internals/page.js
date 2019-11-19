// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cr.ui.pageManager', function() {
  const PageManager = cr.ui.pageManager.PageManager;

  /**
   * Base class for pages that can be shown and hidden by PageManager. Each Page
   * is like a node in a forest, corresponding to a particular div. At any
   * point, one root Page is visible, and any visible Page can show a child Page
   * as an overlay. The host of the root Page(s) should provide a container div
   * for each nested level to enforce the stack order of overlays.
   */
  class Page extends cr.EventTarget {
    /**
     * @param {string} name Page name.
     * @param {string} title Page title, used for history.
     * @param {string} pageDivName ID of the div corresponding to the page.
     */
    constructor(name, title, pageDivName) {
      super();

      this.name = name;
      this.title = title;
      this.pageDivName = pageDivName;
      this.pageDiv = getRequiredElement(this.pageDivName);
      // |pageDiv.page| is set to the page object (this) when the page is
      // visible to track which page is being shown when multiple pages can
      // share the same underlying div.
      this.pageDiv.page = null;
      this.tab = null;
      this.lastFocusedElement = null;
      this.hash = '';

      /**
       * The parent page of this page; or null for root pages.
       * @type {cr.ui.pageManager.Page}
       */
      this.parentPage = null;

      /**
       * The section on the parent page that is associated with this page.
       * Can be null.
       * @type {Element}
       */
      this.associatedSection = null;

      /**
       * An array of controls that are associated with this page. The first
       * control should be located on a root page.
       * @type {Array<Element>}
       */
      this.associatedControls = null;

      /**
       * If true; this page should always be considered the top-most page when
       * visible.
       * @private {boolean}
       */
      this.alwaysOnTop_ = false;

      /**
       * Set this to handle cancelling an overlay (and skip some typical steps).
       * @see {cr.ui.PageManager.prototype.cancelOverlay}
       * @type {?Function}
       */
      this.handleCancel = null;

      /** @type {boolean} */
      this.isOverlay = false;
    }

    /**
     * Initializes page content.
     */
    initializePage() {}

    /**
     * Called by the PageManager when this.hash changes while the page is
     * already visible. This is analogous to the hashchange DOM event.
     */
    didChangeHash() {}

    /**
     * Sets focus on the first focusable element. Override for a custom focus
     * strategy.
     */
    focus() {
      cr.ui.setInitialFocus(this.pageDiv);
    }

    /**
     * Reverse any buttons strips in this page (only applies to overlays).
     * @see cr.ui.reverseButtonStrips for an explanation of why this is
     * necessary and when it's done.
     */
    reverseButtonStrip() {
      assert(this.isOverlay);
      cr.ui.reverseButtonStrips(this.pageDiv);
    }

    /**
     * Whether it should be possible to show the page.
     * @return {boolean} True if the page should be shown.
     */
    canShowPage() {
      return true;
    }

    /**
     * Updates the hash of the current page. If the page is topmost, the history
     * state is updated.
     * @param {string} hash The new hash value. Like location.hash, this
     *     should include the leading '#' if not empty.
     */
    setHash(hash) {
      if (this.hash == hash) {
        return;
      }
      this.hash = hash;
      PageManager.onPageHashChanged(this);
    }

    /**
     * Called after the page has been shown.
     */
    didShowPage() {}

    /**
     * Called before the page will be hidden, e.g., when a different root page
     * will be shown.
     */
    willHidePage() {}

    /**
     * Called after the overlay has been closed.
     */
    didClosePage() {}

    /**
     * Gets the container div for this page if it is an overlay.
     * @type {HTMLDivElement}
     */
    get container() {
      assert(this.isOverlay);
      return this.pageDiv.parentNode;
    }

    /**
     * Gets page visibility state.
     * @type {boolean}
     */
    get visible() {
      // If this is an overlay dialog it is no longer considered visible while
      // the overlay is fading out. See http://crbug.com/118629.
      if (this.isOverlay && this.container.classList.contains('transparent')) {
        return false;
      }
      if (this.pageDiv.hidden) {
        return false;
      }
      return this.pageDiv.page == this;
    }

    /**
     * Sets page visibility.
     * @type {boolean}
     */
    set visible(visible) {
      if ((this.visible && visible) || (!this.visible && !visible)) {
        return;
      }

      // If using an overlay, the visibility of the dialog is toggled at the
      // same time as the overlay to show the dialog's out transition. This
      // is handled in setOverlayVisible.
      if (this.isOverlay) {
        this.setOverlayVisible_(visible);
      } else {
        this.pageDiv.page = this;
        this.pageDiv.hidden = !visible;
        PageManager.onPageVisibilityChanged(this);
      }

      cr.dispatchPropertyChange(this, 'visible', visible, !visible);
    }

    /**
     * Whether the page is considered 'sticky', such that it will remain a root
     * page even if sub-pages change.
     * @type {boolean} True if this page is sticky.
     */
    get sticky() {
      return false;
    }

    /**
     * @type {boolean} True if this page should always be considered the
     *     top-most page when visible.
     */
    get alwaysOnTop() {
      return this.alwaysOnTop_;
    }

    /**
     * @type {boolean} True if this page should always be considered the
     *     top-most page when visible. Only overlays can be always on top.
     */
    set alwaysOnTop(value) {
      assert(this.isOverlay);
      this.alwaysOnTop_ = value;
    }

    /**
     * Shows or hides an overlay (including any visible dialog).
     * @param {boolean} visible Whether the overlay should be visible or not.
     * @private
     */
    setOverlayVisible_(visible) {
      assert(this.isOverlay);
      const pageDiv = this.pageDiv;
      const container = this.container;

      if (container.hidden != visible) {
        if (visible) {
          // If the container is set hidden and then immediately set visible
          // again, the fadeCompleted_ callback would cause it to be erroneously
          // hidden again. Removing the transparent tag avoids that.
          container.classList.remove('transparent');

          // Hide all dialogs in this container since a different one may have
          // been previously visible before fading out.
          const pages = container.querySelectorAll('.page');
          for (let i = 0; i < pages.length; i++) {
            pages[i].hidden = true;
          }
          // Show the new dialog.
          pageDiv.hidden = false;
          pageDiv.page = this;
        }
        return;
      }

      const self = this;
      const loading = PageManager.isLoading();
      if (!loading) {
        // TODO(flackr): Use an event delegate to avoid having to subscribe and
        // unsubscribe for transitionend events.
        container.addEventListener('transitionend', function f(e) {
          const propName = e.propertyName;
          if (e.target != e.currentTarget ||
              (propName && propName != 'opacity')) {
            return;
          }
          container.removeEventListener('transitionend', f);
          self.fadeCompleted_();
        });
        // transition is 200ms. Let's wait for 400ms.
        ensureTransitionEndEvent(container, 400);
      }

      if (visible) {
        container.hidden = false;
        pageDiv.hidden = false;
        pageDiv.page = this;
        // NOTE: This is a hacky way to force the container to layout which
        // will allow us to trigger the transition.
        /** @suppress {uselessCode} */
        container.scrollTop;

        this.pageDiv.removeAttribute('aria-hidden');
        if (this.parentPage) {
          this.parentPage.pageDiv.parentElement.setAttribute(
              'aria-hidden', true);
        }
        container.classList.remove('transparent');
        PageManager.onPageVisibilityChanged(this);
      } else {
        // Kick change events for text fields.
        if (pageDiv.contains(document.activeElement)) {
          document.activeElement.blur();
        }
        container.classList.add('transparent');
      }

      if (loading) {
        this.fadeCompleted_();
      }
    }

    /**
     * Called when a container opacity transition finishes.
     * @private
     */
    fadeCompleted_() {
      if (this.container.classList.contains('transparent')) {
        this.pageDiv.hidden = true;
        this.container.hidden = true;

        if (this.parentPage) {
          this.parentPage.pageDiv.parentElement.removeAttribute('aria-hidden');
        }

        PageManager.onPageVisibilityChanged(this);
      }
    }
  }

  // Export
  return {Page: Page};
});
