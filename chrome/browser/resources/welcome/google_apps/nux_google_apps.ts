// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../shared/step_indicator.js';
import '../strings.m.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {NavigationMixin} from '../navigation_mixin.js';
import {navigateToNextStep} from '../router.js';
import type {BookmarkProxy} from '../shared/bookmark_proxy.js';
import {BookmarkBarManager, BookmarkProxyImpl} from '../shared/bookmark_proxy.js';
import {ModuleMetricsManager} from '../shared/module_metrics_proxy.js';
import type {StepIndicatorModel} from '../shared/nux_types.js';

import type {GoogleAppProxy} from './google_app_proxy.js';
import {GoogleAppProxyImpl} from './google_app_proxy.js';
import {GoogleAppsMetricsProxyImpl} from './google_apps_metrics_proxy.js';
import {getCss} from './nux_google_apps.css.js';
import {getHtml} from './nux_google_apps.html.js';

interface AppItem {
  id: number;
  name: string;
  icon: string;
  url: string;
  bookmarkId: string|null;
  selected: boolean;
}

const KEYBOARD_FOCUSED = 'keyboard-focused';

export interface NuxGoogleAppsElement {
  $: {
    noThanksButton: HTMLElement,
  };
}

const NuxGoogleAppsElementBase = I18nMixinLit(NavigationMixin(CrLitElement));

export class NuxGoogleAppsElement extends NuxGoogleAppsElementBase {
  static get is() {
    return 'nux-google-apps';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      indicatorModel: {type: Object},
      appList_: {type: Array},
      hasAppsSelected_: {type: Boolean},
    };
  }

  private appProxy_: GoogleAppProxy;
  private metricsManager_: ModuleMetricsManager;
  private finalized_: boolean = false;
  private bookmarkProxy_: BookmarkProxy;
  private bookmarkBarManager_: BookmarkBarManager;
  private wasBookmarkBarShownOnInit_: boolean = false;
  protected appList_: AppItem[] = [];
  protected hasAppsSelected_: boolean = true;
  indicatorModel?: StepIndicatorModel;

  constructor() {
    super();

    this.subtitle = loadTimeData.getString('googleAppsDescription');
    this.appProxy_ = GoogleAppProxyImpl.getInstance();
    this.metricsManager_ =
        new ModuleMetricsManager(GoogleAppsMetricsProxyImpl.getInstance());
    this.bookmarkProxy_ = BookmarkProxyImpl.getInstance();
    this.bookmarkBarManager_ = BookmarkBarManager.getInstance();
  }

  override onRouteEnter() {
    this.finalized_ = false;
    this.metricsManager_.recordPageInitialized();
    this.populateAllBookmarks_();
  }

  override onRouteExit() {
    if (this.finalized_) {
      return;
    }
    this.cleanUp_();
    this.metricsManager_.recordBrowserBackOrForward();
  }

  override onRouteUnload() {
    if (this.finalized_) {
      return;
    }
    this.cleanUp_();
    this.metricsManager_.recordNavigatedAway();
  }

  private changeFocus_(element: EventTarget, direction: number) {
    if (isRTL()) {
      direction *= -1;  // Reverse direction if RTL.
    }

    const buttons = this.shadowRoot!.querySelectorAll('button');
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

  private announceA11y_(text: string) {
    getAnnouncerInstance().announce(text);
  }

  /**
   * Called when bookmarks should be removed for all selected apps.
   */
  private cleanUp_() {
    this.finalized_ = true;

    if (this.appList_.length === 0) {
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
   */
  protected onAppClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const item = this.appList_[index]!;

    item.selected = !item.selected;
    this.requestUpdate();

    this.updateBookmark_(item);
    this.updateHasAppsSelected_();

    this.metricsManager_.recordClickedOption();

    // Announcements should NOT be in |updateBookmark_| because there should be
    // a different utterance when all app bookmarks are added/removed.
    const i18nKey = item.selected ? 'bookmarkAdded' : 'bookmarkRemoved';
    this.announceA11y_(this.i18n(i18nKey));
  }

  protected onAppKeyUp_(e: KeyboardEvent) {
    if (e.key === 'ArrowRight') {
      this.changeFocus_(e.currentTarget!, 1);
    } else if (e.key === 'ArrowLeft') {
      this.changeFocus_(e.currentTarget!, -1);
    } else {
      (e.currentTarget as HTMLElement).classList.add(KEYBOARD_FOCUSED);
    }
  }

  protected onAppPointerDown_(e: Event) {
    (e.currentTarget as HTMLElement).classList.remove(KEYBOARD_FOCUSED);
  }

  protected onNextClicked_() {
    this.finalized_ = true;
    this.appList_.forEach(app => {
      if (app.selected) {
        this.appProxy_.recordProviderSelected(app.id);
      }
    });
    this.metricsManager_.recordGetStarted();
    navigateToNextStep();
  }

  protected onNoThanksClicked_() {
    this.cleanUp_();
    this.metricsManager_.recordNoThanks();
    navigateToNextStep();
  }

  /**
   * Called when bookmarks should be created for all selected apps.
   */
  private populateAllBookmarks_() {
    this.wasBookmarkBarShownOnInit_ = this.bookmarkBarManager_.getShown();

    if (this.appList_.length > 0) {
      this.appList_.forEach(app => this.updateBookmark_(app));
    } else {
      this.appProxy_.getAppList().then(list => {
        this.appList_ = list as AppItem[];
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

  private updateBookmark_(item: AppItem) {
    if (item.selected && !item.bookmarkId) {
      this.bookmarkBarManager_.setShown(true);
      this.bookmarkProxy_
          .addBookmark({
            title: item.name,
            url: item.url,
            parentId: '1',
          })
          .then(result => {
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
   */
  private updateHasAppsSelected_() {
    this.hasAppsSelected_ = this.appList_.some(a => a.selected);
    if (!this.hasAppsSelected_) {
      this.bookmarkBarManager_.setShown(this.wasBookmarkBarShownOnInit_);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'nux-google-apps': NuxGoogleAppsElement;
  }
}

customElements.define(NuxGoogleAppsElement.is, NuxGoogleAppsElement);
