// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

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
      if (x.location == y.location)
        return 0;
      if (x.location == chrome.developerPrivate.Location.UNPACKED)
        return -1;
      if (y.location == chrome.developerPrivate.Location.UNPACKED)
        return 1;
      return 0;
    }
    return compareLocation(a, b) ||
        compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
        compare(a.id, b.id);
  };

  const Manager = Polymer({
    is: 'extensions-manager',

    properties: {
      canLoadUnpacked: {
        type: Boolean,
        value: false,
      },

      /** @type {!extensions.Service} */
      delegate: {
        type: Object,
        value: function() {
          return extensions.Service.getInstance();
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
      'view-exit-finish': 'onViewExitFinish_',
    },

    /**
     * The current page being shown. Default to null, and initPage_ will figure
     * out the initial page based on url.
     * @private {?PageState}
     */
    currentPage_: null,

    /**
     * The ID of the listener on |extensions.navigation|. Stored so that the
     * listener can be removed when this element is detached (happens in tests).
     * @private {?number}
     */
    navigationListener_: null,

    /** @override */
    ready: function() {
      let service = extensions.Service.getInstance();

      let onProfileStateChanged = profileInfo => {
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
      extensions.KioskBrowserProxyImpl.getInstance()
          .initializeKioskAppSettings()
          .then(params => {
            this.kioskEnabled_ = params.kioskEnabled;
          });
      // </if>
    },

    /** @override */
    attached: function() {
      document.documentElement.classList.remove('loading');
      document.fonts.load('bold 12px Roboto');

      this.navigationListener_ = extensions.navigation.addListener(newPage => {
        this.changePage_(newPage);
      });
    },

    /** @override */
    detached: function() {
      assert(extensions.navigation.removeListener(this.navigationListener_));
      this.navigationListener_ = null;
    },

    /**
     * Initializes the page to reflect what's specified in the url so that if
     * the user visits chrome://extensions/?id=..., we land on the proper page.
     * @private
     */
    initPage_: function() {
      this.didInitPage_ = true;
      this.changePage_(extensions.navigation.getCurrentPage());
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
          if (!eventData.extensionInfo)
            break;

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
     * @param {!CustomEvent} event
     * @private
     */
    onFilterChanged_: function(event) {
      if (this.currentPage_.page !== Page.LIST)
        extensions.navigation.navigateTo({page: Page.LIST});
      this.filter = /** @type {string} */ (event.detail);
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
          assertNotReached(
              'Don\'t send themes to the chrome://extensions page');
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
      let apps = [];
      let extensions = [];
      for (let i of extensionsAndApps) {
        let list = this.getListId_(i) === 'apps_' ? apps : extensions;
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
      if (insertBeforeChild == -1)
        insertBeforeChild = this[listId].length;
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
        extensions.navigation.replaceWith({page: Page.LIST});
      }
    },

    /**
     * @param {!CustomEvent} e
     * @private
     */
    onLoadError_: function(e) {
      const loadError =
          /** @type {!chrome.developerPrivate.LoadError} */ (e.detail);
      this.showLoadErrorDialog_ = true;
      this.async(() => {
        const dialog = this.$$('#load-error');
        dialog.loadError = loadError;
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
        optionsDialog.close();
        this.showOptionsDialog_ = false;
      }

      const fromPage = this.currentPage_ ? this.currentPage_.page : null;
      const toPage = newPage.page;
      let data;
      if (newPage.extensionId) {
        data = this.getData_(newPage.extensionId);
        if (!data) {
          // Attempting to view an invalid (removed?) app or extension ID.
          extensions.navigation.replaceWith({page: Page.LIST});
          return;
        }
      }

      if (toPage == Page.DETAILS)
        this.detailViewItem_ = assert(data);
      else if (toPage == Page.ERRORS)
        this.errorPageItem_ = assert(data);
      else if (toPage == Page.ACTIVITY_LOG) {
        if (!this.showActivityLog) {
          // Redirect back to the details page if we try to view the
          // activity log of an extension but the flag is not set.
          extensions.navigation.replaceWith(
              {page: Page.DETAILS, extensionId: newPage.extensionId});
          return;
        }
      }

      if (fromPage != toPage) {
        /** @type {CrViewManagerElement} */ (this.$.viewManager)
            .switchView(toPage);
      }

      if (newPage.subpage) {
        assert(newPage.subpage == Dialog.OPTIONS);
        assert(newPage.extensionId);
        this.showOptionsDialog_ = true;
        this.async(() => {
          this.$$('#options-dialog').show(data);
        });
      }

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
    },

    /** @private */
    onViewExitFinish_: function(e) {
      const viewType = e.path[0].tagName;
      if (viewType == 'EXTENSIONS-ITEM-LIST' ||
          viewType == 'EXTENSIONS-KEYBOARD-SHORTCUTS' ||
          viewType == 'EXTENSIONS-ACTIVITY-LOG') {
        return;
      }

      const extensionId = e.path[0].data.id;
      const list = this.$$('extensions-item-list');
      const button = viewType == 'EXTENSIONS-DETAIL-VIEW' ?
          list.getDetailsButton(extensionId) :
          list.getErrorsButton(extensionId);

      // The button will not exist, when returning from a details page
      // because the corresponding extension/app was deleted.
      if (button)
        button.focus();
    },

    /**
     * @param {!CustomEvent} e
     * @private
     */
    onShowInstallWarnings_: function(e) {
      // Leverage Polymer data bindings instead of just assigning the
      // installWarnings on the dialog since the dialog hasn't been stamped
      // in the DOM yet.
      this.installWarnings_ = /** @type{!Array<string>} */ (e.detail);
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

  return {Manager: Manager};
});
