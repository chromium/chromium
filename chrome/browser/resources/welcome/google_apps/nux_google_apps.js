// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/cr.m.js';
import 'chrome://resources/js/util.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../shared/animations_css.js';
import '../shared/chooser_shared_css.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigateToNextStep, NavigationBehavior, NavigationBehaviorInterface} from '../navigation_behavior.js';
import {BookmarkBarManager, BookmarkProxy, BookmarkProxyImpl} from '../shared/bookmark_proxy.js';
import {ModuleMetricsManager} from '../shared/module_metrics_proxy.js';
import {BookmarkListItem, stepIndicatorModel} from '../shared/nux_types.js';

import {GoogleAppProxy, GoogleAppProxyImpl} from './google_app_proxy.js';
import {GoogleAppsMetricsProxyImpl} from './google_apps_metrics_proxy.js';

/**
 * @typedef {{
 *   id: number,
 *   name: string,
 *   icon: string,
 *   url: string,
 *   bookmarkId: ?string,
 *   selected: boolean,
 * }}
 */
let AppItem;

/**
 * @typedef {{
 *   item: !AppItem,
 *   set: function(string, boolean):void
 * }}
 */
let AppItemModel;

const KEYBOARD_FOCUSED = 'keyboard-focused';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {NavigationBehaviorInterface}
 */
const NuxGoogleAppsElementBase =
    mixinBehaviors([I18nBehavior, NavigationBehavior], PolymerElement);

/** @polymer */
export class NuxGoogleAppsElement extends NuxGoogleAppsElementBase {
  static get is() {
    return 'nux-google-apps';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {stepIndicatorModel} */
      indicatorModel: Object,

      /** @private {!Array<!AppItem>} */
      appList_: Array,

      /** @private */
      hasAppsSelected_: {
        type: Boolean,
        notify: true,
        value: true,
      },

      subtitle: {
        type: String,
        value: loadTimeData.getString('googleAppsDescription'),
      },
    };
  }

  constructor() {
    super();

    /** @private {GoogleAppProxy} */
    this.appProxy_ = null;

    /** @private {?ModuleMetricsManager} */
    this.metricsManager_ = null;

    /** @private {boolean} */
    this.finalized_ = false;

    /** @private {BookmarkProxy} */
    this.bookmarkProxy_ = null;

    /** @private {BookmarkBarManager} */
    this.bookmarkBarManager_ = null;

    /** @private {boolean} */
    this.wasBookmarkBarShownOnInit_ = false;
  }

  /** @override */
  ready() {
    super.ready();

    this.appProxy_ = GoogleAppProxyImpl.getInstance();
    this.metricsManager_ = new ModuleMetricsManager(
        GoogleAppsMetricsProxyImpl.getInstance());
    this.bookmarkProxy_ = BookmarkProxyImpl.getInstance();
    this.bookmarkBarManager_ = BookmarkBarManager.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    afterNextRender(this, () => IronA11yAnnouncer.requestAvailability());
  }

  onRouteEnter() {
    this.finalized_ = false;
    this.metricsManager_.recordPageInitialized();
    this.populateAllBookmarks_();
  }

  onRouteExit() {
    if (this.finalized_) {
      return;
    }
    this.cleanUp_();
    this.metricsManager_.recordBrowserBackOrForward();
  }

  onRouteUnload() {
    if (this.finalized_) {
      return;
    }
    this.cleanUp_();
    this.metricsManager_.recordNavigatedAway();
  }

  /**
   * @param {EventTarget} element
   * @param {number} direction
   * @private
   */
  changeFocus_(element, direction) {
    if (isRTL()) {
      direction *= -1;  // Reverse direction if RTL.
    }

    const buttons = this.root.querySelectorAll('button');
    const targetIndex = Array.prototype.indexOf.call(buttons, element);

    const oldFocus = buttons[targetIndex];
    if (!oldFocus) {
      return;
    }

    const newFocus = buttons[targetIndex + direction];

    // New target and we're changing direction.
    if (newFocus && direction) {
      newFocus.classList.add(KEYBOARD_FOCUSED);
      oldFocus.classList.remove(KEYBOARD_FOCUSED);
      newFocus.focus();
    } else {
      oldFocus.classList.add(KEYBOARD_FOCUSED);
    }
  }

  /**
   * @param {string} text
   * @private
   */
  announceA11y_(text) {
    this.dispatchEvent(new CustomEvent(
        'iron-announce', {bubbles: true, composed: true, detail: {text}}));
  }

  /**
   * Called when bookmarks should be removed for all selected apps.
   * @private
   */
  cleanUp_() {
    this.finalized_ = true;

    if (!this.appList_) {
      return;
    }  // No apps to remove.

    let removedBookmarks = false;
    this.appList_.forEach(app => {
      if (app.selected && app.bookmarkId) {
        // Don't call |updateBookmark_| b/c we want to save the selection in the
        // event of a browser back/forward.
        this.bookmarkProxy_.removeBookmark(app.bookmarkId);
        app.bookmarkId = null;
        removedBookmarks = true;
      }
    });
    // Only update and announce if we removed bookmarks.
    if (removedBookmarks) {
      this.bookmarkBarManager_.setShown(this.wasBookmarkBarShownOnInit_);
      this.announceA11y_(this.i18n('bookmarksRemoved'));
    }
  }


  /**
   * Handle toggling the apps selected.
   * @param {!{model: !AppItemModel}} e
   * @private
   */
  onAppClick_(e) {
    const item = e.model.item;

    e.model.set('item.selected', !item.selected);

    this.updateBookmark_(item);
    this.updateHasAppsSelected_();

    this.metricsManager_.recordClickedOption();

    // Announcements should NOT be in |updateBookmark_| because there should be
    // a different utterance when all app bookmarks are added/removed.
    const i18nKey = item.selected ? 'bookmarkAdded' : 'bookmarkRemoved';
    this.announceA11y_(this.i18n(i18nKey));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onAppKeyUp_(e) {
    if (e.key === 'ArrowRight') {
      this.changeFocus_(e.currentTarget, 1);
    } else if (e.key === 'ArrowLeft') {
      this.changeFocus_(e.currentTarget, -1);
    } else {
      e.currentTarget.classList.add(KEYBOARD_FOCUSED);
    }
  }

  /**
   * @param {!Event} e
   * @private
   */
  onAppPointerDown_(e) {
    e.currentTarget.classList.remove(KEYBOARD_FOCUSED);
  }

  /** @private */
  onGetStartedClicked_() {
    this.finalized_ = true;
    this.appList_.forEach(app => {
      if (app.selected) {
        this.appProxy_.recordProviderSelected(app.id);
      }
    });
    this.metricsManager_.recordGetStarted();
    navigateToNextStep();
  }

  /** @private */
  onNoThanksClicked_() {
    this.cleanUp_();
    this.metricsManager_.recordNoThanks();
    navigateToNextStep();
  }

  /**
   * Called when bookmarks should be created for all selected apps.
   * @private
   */
  populateAllBookmarks_() {
    this.wasBookmarkBarShownOnInit_ = this.bookmarkBarManager_.getShown();

    if (this.appList_) {
      this.appList_.forEach(app => this.updateBookmark_(app));
    } else {
      this.appProxy_.getAppList().then(list => {
        this.appList_ = /** @type(!Array<!AppItem>) */ (list);
        this.appList_.forEach((app, index) => {
          // Default select first few items.
          app.selected = index < 3;
          this.updateBookmark_(app);
        });
        this.updateHasAppsSelected_();
        this.announceA11y_(this.i18n('bookmarksAdded'));
      });
    }
  }

  /**
   * @param {!AppItem} item
   * @private
   */
  updateBookmark_(item) {
    if (item.selected && !item.bookmarkId) {
      this.bookmarkBarManager_.setShown(true);
      this.bookmarkProxy_.addBookmark(
          {
            title: item.name,
            url: item.url,
            parentId: '1',
          },
          result => {
            item.bookmarkId = result.id;
          });
      // Cache bookmark icon.
      this.appProxy_.cacheBookmarkIcon(item.id);
    } else if (!item.selected && item.bookmarkId) {
      this.bookmarkProxy_.removeBookmark(item.bookmarkId);
      item.bookmarkId = null;
    }
  }

  /**
   * Updates the value of hasAppsSelected_.
   * @private
   */
  updateHasAppsSelected_() {
    this.hasAppsSelected_ =
        this.appList_ && this.appList_.some(a => a.selected);
    if (!this.hasAppsSelected_) {
      this.bookmarkBarManager_.setShown(this.wasBookmarkBarShownOnInit_);
    }
  }

  /**
   * Converts a boolean to a string because aria-pressed needs a string value.
   * @param {boolean} value
   * @return {string}
   * @private
   */
  getAriaPressed_(value) {
    return value ? 'true' : 'false';
  }
}
customElements.define(NuxGoogleAppsElement.is, NuxGoogleAppsElement);
