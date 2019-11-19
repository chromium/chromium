// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.m.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.m.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './detail_view.js';
import './drop_overlay.js';
import './error_page.js';
import './install_warnings_dialog.js';
import './item_list.js';
import './item_util.js';
import './keyboard_shortcuts.js';
import './load_error.js';
import './options_dialog.js';
import './sidebar.js';
import './toolbar.js';
// <if expr="chromeos">
import './kiosk_dialog.js';
// </if>

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActivityLogExtensionPlaceholder} from './activity_log/activity_log.js';
// <if expr="chromeos">
import {KioskBrowserProxyImpl} from './kiosk_browser_proxy.js';
// </if>
import {Dialog, navigation, Page, PageState} from './navigation_helper.js';
import {Service} from './service.js';

/**
 * Compares two extensions to determine which should come first in the list.
 * @param {chrome.developerPrivate.ExtensionInfo} a
 * @param {chrome.developerPrivate.ExtensionInfo} b
 * @return {number}
 */
const compareExtensions = function(a, b) {
  function compare(x, y) {
    return x < y ? -1 : (x > y ? 1 : 0);
  }
  function compareLocation(x, y) {
    if (x.location == y.location) {
      return 0;
    }
    if (x.location == chrome.developerPrivate.Location.UNPACKED) {
      return -1;
    }
    if (y.location == chrome.developerPrivate.Location.UNPACKED) {
      return 1;
    }
    return 0;
  }
  return compareLocation(a, b) ||
      compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
      compare(a.id, b.id);
};

Polymer({
  is: 'extensions-manager',

  _template: html`{__html_template__}`,

  properties: {
    canLoadUnpacked: {
      type: Boolean,
      value: false,
    },

    /** @type {!Service} */
    delegate: {
      type: Object,
      value: function() {
        return Service.getInstance();
      },
    },

    inDevMode: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('inDevMode'),
    },

    showActivityLog: {
      type: Boolean,
      value: () => loadTimeData.getBoolean('showActivityLog'),
    },

    devModeControlledByPolicy: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isSupervised_: {
      type: Boolean,
      value: false,
    },

    incognitoAvailable_: {
      type: Boolean,
      value: false,
    },

    filter: {
      type: String,
      value: '',
    },

    /**
     * The item currently displayed in the error subpage. We use a separate
     * item for different pages (rather than a single subpageItem_ property)
     * so that hidden subpages don't update when an item updates. That is, we
     * don't want the details view subpage to update when the item shown in
     * the errors page updates, and vice versa.
     * @private {!chrome.developerPrivate.ExtensionInfo|undefined}
     */
    errorPageItem_: Object,

    /**
     * The item currently displayed in the details view subpage. See also
     * errorPageItem_.
     * @private {!chrome.developerPrivate.ExtensionInfo|undefined}
     */
    detailViewItem_: Object,

    /**
     * The item that provides some information about the current extension
     * for the activity log view subpage. See also errorPageItem_.
     * @private {!chrome.developerPrivate.ExtensionInfo|undefined|
     *           !ActivityLogExtensionPlaceholder}
     */
    activityLogItem_: Object,

    /** @private {!Array<!chrome.developerPrivate.ExtensionInfo>} */
    extensions_: Array,

    /** @private {!Array<!chrome.developerPrivate.ExtensionInfo>} */
    apps_: Array,

    /**
     * Prevents page content from showing before data is first loaded.
     * @private
     */
    didInitPage_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showDrawer_: Boolean,

    /** @private */
    showLoadErrorDialog_: Boolean,

    /** @private */
    showInstallWarningsDialog_: Boolean,

    /** @private {?Array<string>} */
    installWarnings_: Array,

    /** @private */
    showOptionsDialog_: Boolean,

    /**
     * Whether the last page the user navigated from was the activity log
     * page.
     * @private
     */
    fromActivityLog_: Boolean,

    // <if expr="chromeos">
    /** @private */
    kioskEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showKioskDialog_: {
      type: Boolean,
      value: false,
    },
    // </if>
  },

  listeners: {
    'load-error': 'onLoadError_',
    'view-enter-start': 'onViewEnterStart_',
    'view-exit-start': 'onViewExitStart_',
    'view-exit-finish': 'onViewExitFinish_',
  },

  /**
   * The current page being shown. Default to null, and initPage_ will figure
   * out the initial page based on url.
   * @private {?PageState}
   */
  currentPage_: null,

  /**
   * The ID of the listener on |navigation|. Stored so that the
   * listener can be removed when this element is detached (happens in tests).
   * @private {?number}
   */
  navigationListener_: null,

  /** @override */
  ready: function() {
    const service = Service.getInstance();

    const onProfileStateChanged = profileInfo => {
      this.isSupervised_ = profileInfo.isSupervised;
      this.incognitoAvailable_ = profileInfo.isIncognitoAvailable;
      this.devModeControlledByPolicy =
          profileInfo.isDeveloperModeControlledByPolicy;
      this.inDevMode = profileInfo.inDeveloperMode;
      this.canLoadUnpacked = profileInfo.canLoadUnpacked;
    };
    service.getProfileStateChangedTarget().addListener(onProfileStateChanged);
    service.getProfileConfiguration().then(onProfileStateChanged);

    service.getExtensionsInfo().then(extensionsAndApps => {
      this.initExtensionsAndApps_(extensionsAndApps);
      this.initPage_();

      service.getItemStateChangedTarget().addListener(
          this.onItemStateChanged_.bind(this));
    });

    // <if expr="chromeos">
    KioskBrowserProxyImpl.getInstance().initializeKioskAppSettings().then(
        params => {
          this.kioskEnabled_ = params.kioskEnabled;
        });
    // </if>
  },

  /** @override */
  attached: function() {
    document.documentElement.classList.remove('loading');
    document.fonts.load('bold 12px Roboto');

    this.navigationListener_ = navigation.addListener(newPage => {
      this.changePage_(newPage);
    });
  },

  /** @override */
  detached: function() {
    assert(navigation.removeListener(
        /** @type {number} */ (this.navigationListener_)));
    this.navigationListener_ = null;
  },

  /**
   * Initializes the page to reflect what's specified in the url so that if
   * the user visits chrome://extensions/?id=..., we land on the proper page.
   * @private
   */
  initPage_: function() {
    this.didInitPage_ = true;
    this.changePage_(navigation.getCurrentPage());
  },

  /**
   * @param {!chrome.developerPrivate.EventData} eventData
   * @private
   */
  onItemStateChanged_: function(eventData) {
    const EventType = chrome.developerPrivate.EventType;
    switch (eventData.event_type) {
      case EventType.VIEW_REGISTERED:
      case EventType.VIEW_UNREGISTERED:
      case EventType.INSTALLED:
      case EventType.LOADED:
      case EventType.UNLOADED:
      case EventType.ERROR_ADDED:
      case EventType.ERRORS_REMOVED:
      case EventType.PREFS_CHANGED:
      case EventType.WARNINGS_CHANGED:
      case EventType.COMMAND_ADDED:
      case EventType.COMMAND_REMOVED:
      case EventType.PERMISSIONS_CHANGED:
        // |extensionInfo| can be undefined in the case of an extension
        // being unloaded right before uninstallation. There's nothing to do
        // here.
        if (!eventData.extensionInfo) {
          break;
        }

        if (this.delegate.shouldIgnoreUpdate(
                eventData.extensionInfo.id, eventData.event_type)) {
          break;
        }

        const listId = this.getListId_(eventData.extensionInfo);
        const currentIndex = this[listId].findIndex(
            item => item.id == eventData.extensionInfo.id);

        if (currentIndex >= 0) {
          this.updateItem_(listId, currentIndex, eventData.extensionInfo);
        } else {
          this.addItem_(listId, eventData.extensionInfo);
        }
        break;
      case EventType.UNINSTALLED:
        this.removeItem_(eventData.item_id);
        break;
      default:
        assertNotReached();
    }
  },

  /**
   * @param {!CustomEvent<string>} event
   * @private
   */
  onFilterChanged_: function(event) {
    if (this.currentPage_.page !== Page.LIST) {
      navigation.navigateTo({page: Page.LIST});
    }
    this.filter = event.detail;
  },

  /** @private */
  onMenuButtonTap_: function() {
    this.showDrawer_ = true;
    this.async(() => {
      this.$$('#drawer').openDrawer();
    });
  },

  /**
   * @param {!chrome.developerPrivate.ExtensionInfo} item
   * @return {string} The ID of the list that the item belongs in.
   * @private
   */
  getListId_: function(item) {
    const ExtensionType = chrome.developerPrivate.ExtensionType;
    switch (item.type) {
      case ExtensionType.HOSTED_APP:
      case ExtensionType.LEGACY_PACKAGED_APP:
      case ExtensionType.PLATFORM_APP:
        return 'apps_';
      case ExtensionType.EXTENSION:
      case ExtensionType.SHARED_MODULE:
        return 'extensions_';
      case ExtensionType.THEME:
        assertNotReached('Don\'t send themes to the chrome://extensions page');
        break;
    }
    assertNotReached();
  },

  /**
   * @param {string} listId The list to look for the item in.
   * @param {string} itemId The id of the item to look for.
   * @return {number} The index of the item in the list, or -1 if not found.
   * @private
   */
  getIndexInList_: function(listId, itemId) {
    return this[listId].findIndex(function(item) {
      return item.id == itemId;
    });
  },

  /**
   * @return {?chrome.developerPrivate.ExtensionInfo}
   * @private
   */
  getData_: function(id) {
    return this.extensions_[this.getIndexInList_('extensions_', id)] ||
        this.apps_[this.getIndexInList_('apps_', id)];
  },

  /**
   * Categorizes |extensionsAndApps| to apps and extensions and initializes
   * those lists.
   * @param {!Array<!chrome.developerPrivate.ExtensionInfo>} extensionsAndApps
   * @private
   */
  initExtensionsAndApps_: function(extensionsAndApps) {
    extensionsAndApps.sort(compareExtensions);
    const apps = [];
    const extensions = [];
    for (const i of extensionsAndApps) {
      const list = this.getListId_(i) === 'apps_' ? apps : extensions;
      list.push(i);
    }

    this.apps_ = apps;
    this.extensions_ = extensions;
  },

  /**
   * Creates and adds a new extensions-item element to the list, inserting it
   * into its sorted position in the relevant section.
   * @param {!chrome.developerPrivate.ExtensionInfo} item The extension
   *     the new element is representing.
   * @private
   */
  addItem_: function(listId, item) {
    // We should never try and add an existing item.
    assert(this.getIndexInList_(listId, item.id) == -1);
    let insertBeforeChild = this[listId].findIndex(function(listEl) {
      return compareExtensions(listEl, item) > 0;
    });
    if (insertBeforeChild == -1) {
      insertBeforeChild = this[listId].length;
    }
    this.splice(listId, insertBeforeChild, 0, item);
  },

  /**
   * @param {!chrome.developerPrivate.ExtensionInfo} item The data for the
   *     item to update.
   * @private
   */
  updateItem_: function(listId, index, item) {
    // We should never try and update a non-existent item.
    assert(index >= 0);
    this.set([listId, index], item);

    // Update the subpage if it is open and displaying the item. If it's not
    // open, we don't update the data even if it's displaying that item. We'll
    // set the item correctly before opening the page. It's a little weird
    // that the DOM will have stale data, but there's no point in causing the
    // extra work.
    if (this.detailViewItem_ && this.detailViewItem_.id == item.id &&
        this.currentPage_.page == Page.DETAILS) {
      this.detailViewItem_ = item;
    } else if (
        this.errorPageItem_ && this.errorPageItem_.id == item.id &&
        this.currentPage_.page == Page.ERRORS) {
      this.errorPageItem_ = item;
    } else if (
        this.activityLogItem_ && this.activityLogItem_.id == item.id &&
        this.currentPage_.page == Page.ACTIVITY_LOG) {
      this.activityLogItem_ = item;
    }
  },

  /**
   * @param {string} itemId The id of item to remove.
   * @private
   */
  removeItem_: function(itemId) {
    // Search for the item to be deleted in |extensions_|.
    let listId = 'extensions_';
    let index = this.getIndexInList_(listId, itemId);
    if (index == -1) {
      // If not in |extensions_| it must be in |apps_|.
      listId = 'apps_';
      index = this.getIndexInList_(listId, itemId);
    }

    // We should never try and remove a non-existent item.
    assert(index >= 0);
    this.splice(listId, index, 1);
    if ((this.currentPage_.page == Page.ACTIVITY_LOG ||
         this.currentPage_.page == Page.DETAILS ||
         this.currentPage_.page == Page.ERRORS) &&
        this.currentPage_.extensionId == itemId) {
      // Leave the details page (the 'list' page is a fine choice).
      navigation.replaceWith({page: Page.LIST});
    }
  },

  /**
   * @param {!CustomEvent<!chrome.developerPrivate.LoadError>} e
   * @private
   */
  onLoadError_: function(e) {
    this.showLoadErrorDialog_ = true;
    this.async(() => {
      const dialog = this.$$('#load-error');
      dialog.loadError = e.detail;
      dialog.show();
    });
  },

  /**
   * Changes the active page selection.
   * @param {PageState} newPage
   * @private
   */
  changePage_: function(newPage) {
    this.onCloseDrawer_();

    const optionsDialog = this.$$('#options-dialog');
    if (optionsDialog && optionsDialog.open) {
      this.showOptionsDialog_ = false;
    }

    const fromPage = this.currentPage_ ? this.currentPage_.page : null;
    const toPage = newPage.page;
    let data;
    let activityLogPlaceholder;
    if (newPage.extensionId) {
      data = this.getData_(newPage.extensionId);
      if (!data) {
        // Allow the user to navigate to the activity log page even if the
        // extension ID is not valid. This enables the use case of seeing an
        // extension's install-time activities by navigating to an extension's
        // activity log page, then installing the extension.
        if (this.showActivityLog && toPage == Page.ACTIVITY_LOG) {
          activityLogPlaceholder = {
            id: newPage.extensionId,
            isPlaceholder: true,
          };
        } else {
          // Attempting to view an invalid (removed?) app or extension ID.
          navigation.replaceWith({page: Page.LIST});
          return;
        }
      }
    }

    if (toPage == Page.DETAILS) {
      this.detailViewItem_ = assert(data);
    } else if (toPage == Page.ERRORS) {
      this.errorPageItem_ = assert(data);
    } else if (toPage == Page.ACTIVITY_LOG) {
      if (!this.showActivityLog) {
        // Redirect back to the details page if we try to view the
        // activity log of an extension but the flag is not set.
        navigation.replaceWith(
            {page: Page.DETAILS, extensionId: newPage.extensionId});
        return;
      }

      this.activityLogItem_ = data ? assert(data) : activityLogPlaceholder;
    }

    if (fromPage != toPage) {
      /** @type {CrViewManagerElement} */ (this.$.viewManager)
          .switchView(/** @type {string} */ (toPage));
    }

    if (newPage.subpage) {
      assert(newPage.subpage == Dialog.OPTIONS);
      assert(newPage.extensionId);
      this.showOptionsDialog_ = true;
      this.async(() => {
        this.$$('#options-dialog').show(data);
      });
    }

    document.title = toPage == Page.DETAILS ?
        `${loadTimeData.getString('title')} - ${this.detailViewItem_.name}` :
        loadTimeData.getString('title');
    this.currentPage_ = newPage;
  },

  /**
   * This method detaches the drawer dialog completely. Should only be
   * triggered by the dialog's 'close' event.
   * @private
   */
  onDrawerClose_: function() {
    this.showDrawer_ = false;
  },

  /**
   * This method animates the closing of the drawer.
   * @private
   */
  onCloseDrawer_: function() {
    const drawer = this.$$('#drawer');
    if (drawer && drawer.open) {
      drawer.close();
    }
  },

  /** @private */
  onLoadErrorDialogClose_: function() {
    this.showLoadErrorDialog_ = false;
  },

  /** @private */
  onOptionsDialogClose_: function() {
    this.showOptionsDialog_ = false;
    this.$$('extensions-detail-view').focusOptionsButton();
  },

  /** @private */
  onViewEnterStart_: function() {
    this.fromActivityLog_ = false;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onViewExitStart_: function(e) {
    const viewType = e.composedPath()[0].tagName;
    this.fromActivityLog_ = viewType == 'EXTENSIONS-ACTIVITY-LOG';
  },

  /**
   * @param {!Event} e
   * @private
   */
  onViewExitFinish_: function(e) {
    const viewType = e.composedPath()[0].tagName;
    if (viewType == 'EXTENSIONS-ITEM-LIST' ||
        viewType == 'EXTENSIONS-KEYBOARD-SHORTCUTS' ||
        viewType == 'EXTENSIONS-ACTIVITY-LOG') {
      return;
    }

    const extensionId = e.composedPath()[0].data.id;
    const list = this.$$('extensions-item-list');
    const button = viewType == 'EXTENSIONS-DETAIL-VIEW' ?
        list.getDetailsButton(extensionId) :
        list.getErrorsButton(extensionId);

    // The button will not exist, when returning from a details page
    // because the corresponding extension/app was deleted.
    if (button) {
      button.focus();
    }
  },

  /**
   * @param {!CustomEvent<!Array<string>>} e
   * @private
   */
  onShowInstallWarnings_: function(e) {
    // Leverage Polymer data bindings instead of just assigning the
    // installWarnings on the dialog since the dialog hasn't been stamped
    // in the DOM yet.
    this.installWarnings_ = e.detail;
    this.showInstallWarningsDialog_ = true;
  },

  /** @private */
  onInstallWarningsDialogClose_: function() {
    this.installWarnings_ = null;
    this.showInstallWarningsDialog_ = false;
  },

  // <if expr="chromeos">
  /** @private */
  onKioskTap_: function() {
    this.showKioskDialog_ = true;
  },

  onKioskDialogClose_: function() {
    this.showKioskDialog_ = false;
  },
  // </if>
});
