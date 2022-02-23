// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-main' displays the selected settings page.
 */
import '//resources/cr_components/managed_footnote/managed_footnote.js';
import '//resources/cr_elements/hidden_style_css.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/js/search_highlight_utils.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_about_page/os_about_page.js';
import '../os_settings_page/os_settings_page.js';
import '../../prefs/prefs.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {PromiseResolver} from '//resources/js/promise_resolver.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {OSPageVisibility, osPageVisibility} from '../os_page_visibility.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

/**
 * @typedef {{about: boolean, settings: boolean}}
 */
let MainPageVisibility;

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-main',

  behaviors: [RouteObserverBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    advancedToggleExpanded: {
      type: Boolean,
      notify: true,
    },

    /** @private */
    overscroll_: {
      type: Number,
      observer: 'overscrollChanged_',
    },

    /**
     * Controls which main pages are displayed via dom-ifs, based on the current
     * route.
     * @private {!MainPageVisibility}
     */
    showPages_: {
      type: Object,
      value() {
        return {about: false, settings: false};
      },
    },

    /** @private */
    showingSubpage_: Boolean,

    toolbarSpinnerActive: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!OSPageVisibility}
     */
    pageVisibility: Object,

    showAndroidApps: Boolean,

    showArcvmManageUsb: Boolean,

    showCrostini: Boolean,

    showReset: Boolean,

    showStartup: Boolean,

    showKerberosSection: Boolean,

    havePlayStoreApp: Boolean,
  },

  /** @private */
  overscrollChanged_() {
    if (!this.overscroll_ && this.boundScroll_) {
      this.offsetParent.removeEventListener('scroll', this.boundScroll_);
      window.removeEventListener('resize', this.boundScroll_);
      this.boundScroll_ = null;
    } else if (this.overscroll_ && !this.boundScroll_) {
      this.boundScroll_ = () => {
        if (!this.showingSubpage_) {
          this.setOverscroll_(0);
        }
      };
      this.offsetParent.addEventListener('scroll', this.boundScroll_);
      window.addEventListener('resize', this.boundScroll_);
    }
  },

  /**
   * Sets the overscroll padding. Never forces a scroll, i.e., always leaves
   * any currently visible overflow as-is.
   * @param {number=} opt_minHeight The minimum overscroll height needed.
   * @private
   */
  setOverscroll_(opt_minHeight) {
    const scroller = this.offsetParent;
    if (!scroller) {
      return;
    }
    const overscroll = this.$.overscroll;
    const visibleBottom = scroller.scrollTop + scroller.clientHeight;
    const overscrollBottom = overscroll.offsetTop + overscroll.scrollHeight;
    // How much of the overscroll is visible (may be negative).
    const visibleOverscroll =
        overscroll.scrollHeight - (overscrollBottom - visibleBottom);
    this.overscroll_ =
        Math.max(opt_minHeight || 0, Math.ceil(visibleOverscroll));
  },

  /**
   * Updates the hidden state of the about and settings pages based on the
   * current route.
   * @param {!Route} newRoute
   */
  currentRouteChanged(newRoute) {
    const inAbout =
        routes.ABOUT.contains(Router.getInstance().getCurrentRoute());
    this.showPages_ = {about: inAbout, settings: !inAbout};

    if (!newRoute.isSubpage()) {
      document.title = inAbout ? loadTimeData.getStringF(
                                     'settingsAltPageTitle',
                                     loadTimeData.getString('aboutPageTitle')) :
                                 loadTimeData.getString('settings');
    }
  },

  /** @private */
  onShowingSubpage_() {
    this.showingSubpage_ = true;
  },

  /** @private */
  onShowingMainPage_() {
    this.showingSubpage_ = false;
  },

  /**
   * A handler for the 'showing-section' event fired from os-settings-page,
   * indicating that a section should be scrolled into view as a result of a
   * navigation.
   * @param {!CustomEvent<!HTMLElement>} e
   * @private
   */
  onShowingSection_(e) {
    const section = e.detail;
    // Calculate the height that the overscroll padding should be set to, so
    // that the given section is displayed at the top of the viewport.
    // Find the distance from the section's top to the overscroll.
    const sectionTop = section.offsetParent.offsetTop + section.offsetTop;
    const distance = this.$.overscroll.offsetTop - sectionTop;
    const overscroll = Math.max(0, this.offsetParent.clientHeight - distance);
    this.setOverscroll_(overscroll);
    section.scrollIntoView();
    section.focus();
  },

  /**
   * @return {boolean}
   * @private
   */
  showManagedHeader_() {
    return !this.showingSubpage_ && !this.showPages_.about;
  },
});
