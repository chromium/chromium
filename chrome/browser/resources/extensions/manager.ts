// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import './activity_log/activity_log.js';
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
import './site_permissions/site_permissions.js';
import './site_permissions/site_permissions_by_site.js';
import './toolbar.js';

import {CrContainerShadowMixinLit} from 'chrome://resources/cr_elements/cr_container_shadow_mixin_lit.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ActivityLogExtensionPlaceholder} from './activity_log/activity_log.js';
import type {ExtensionsDetailViewElement} from './detail_view.js';
import type {ExtensionsItemListElement} from './item_list.js';
import {getCss} from './manager.css.js';
import {getHtml} from './manager.html.js';
import type {PageState} from './navigation_helper.js';
import {Dialog, navigation, Page} from './navigation_helper.js';
import {Service} from './service.js';
import type {ServiceInterface} from './service.js';
import type {ExtensionsToolbarElement} from './toolbar.js';

/**
 * Compares two extensions to determine which should come first in the list.
 */
function compareExtensions(
    a: chrome.developerPrivate.ExtensionInfo,
    b: chrome.developerPrivate.ExtensionInfo): number {
  function compare(x: string, y: string): number {
    return x < y ? -1 : (x > y ? 1 : 0);
  }
  function compareLocation(
      x: chrome.developerPrivate.ExtensionInfo,
      y: chrome.developerPrivate.ExtensionInfo): number {
    if (x.location === y.location) {
      return 0;
    }
    if (x.location === chrome.developerPrivate.Location.UNPACKED) {
      return -1;
    }
    if (y.location === chrome.developerPrivate.Location.UNPACKED) {
      return 1;
    }
    return 0;
  }
  return compareLocation(a, b) ||
      compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
      compare(a.id, b.id);
}

declare global {
  interface HTMLElementEventMap {
    'load-error': CustomEvent<Error|chrome.developerPrivate.LoadError>;
  }
}

export interface ExtensionsManagerElement {
  $: {
    toolbar: ExtensionsToolbarElement,
    viewManager: CrViewManagerElement,
    'items-list': ExtensionsItemListElement,
  };
}

// TODO(crbug.com/40270029): Always show a top shadow for the DETAILS, ERRORS and
// SITE_PERMISSIONS_ALL_SITES pages.
const ExtensionsManagerElementBase = CrContainerShadowMixinLit(CrLitElement);

export class ExtensionsManagerElement extends ExtensionsManagerElementBase {
  static get is() {
    return 'extensions-manager';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      canLoadUnpacked: {type: Boolean},
      delegate: {type: Object},
      inDevMode: {type: Boolean},
      isMv2DeprecationNoticeDismissed: {type: Boolean},
      showActivityLog: {type: Boolean},
      enableEnhancedSiteControls: {type: Boolean},
      devModeControlledByPolicy: {type: Boolean},
      isChildAccount_: {type: Boolean},
      incognitoAvailable_: {type: Boolean},
      filter: {type: String},

      /**
       * The item currently displayed in the error subpage. We use a separate
       * item for different pages (rather than a single subpageItem_ property)
       * so that hidden subpages don't update when an item updates. That is, we
       * don't want the details view subpage to update when the item shown in
       * the errors page updates, and vice versa.
       */
      errorPageItem_: {type: Object},

      /**
       * The item currently displayed in the details view subpage. See also
       * errorPageItem_.
       */
      detailViewItem_: {type: Object},

      /**
       * The item that provides some information about the current extension
       * for the activity log view subpage. See also errorPageItem_.
       */
      activityLogItem_: {type: Object},

      extensions_: {type: Array},
      apps_: {type: Array},

      /**
       * Prevents page content from showing before data is first loaded.
       */
      didInitPage_: {type: Boolean},

      narrow_: {type: Boolean},

      showDrawer_: {type: Boolean},
      showLoadErrorDialog_: {type: Boolean},
      showInstallWarningsDialog_: {type: Boolean},
      installWarnings_: {type: Array},
      showOptionsDialog_: {type: Boolean},

      /**
       * Whether the last page the user navigated from was the activity log
       * page.
       */
      fromActivityLog_: {type: Boolean},
    };
  }

  canLoadUnpacked: boolean = false;
  delegate: ServiceInterface = Service.getInstance();
  inDevMode: boolean = loadTimeData.getBoolean('inDevMode');
  isMv2DeprecationNoticeDismissed: boolean =
      loadTimeData.getBoolean('MV2DeprecationNoticeDismissed');
  showActivityLog: boolean = loadTimeData.getBoolean('showActivityLog');
  enableEnhancedSiteControls: boolean =
      loadTimeData.getBoolean('enableEnhancedSiteControls');
  devModeControlledByPolicy: boolean = false;
  protected isChildAccount_: boolean = false;
  protected incognitoAvailable_: boolean = false;
  filter: string = '';
  protected errorPageItem_?: chrome.developerPrivate.ExtensionInfo;
  protected detailViewItem_?: chrome.developerPrivate.ExtensionInfo;
  protected activityLogItem_?: chrome.developerPrivate.ExtensionInfo|
      ActivityLogExtensionPlaceholder;
  protected extensions_: chrome.developerPrivate.ExtensionInfo[] = [];
  protected apps_: chrome.developerPrivate.ExtensionInfo[] = [];
  protected didInitPage_: boolean = false;
  protected narrow_: boolean = false;
  protected showDrawer_: boolean = false;
  protected showLoadErrorDialog_: boolean = false;
  protected showInstallWarningsDialog_: boolean = false;
  protected installWarnings_: string[]|null = null;
  protected showOptionsDialog_: boolean = false;
  protected fromActivityLog_: boolean = false;

  /**
   * A promise resolver for any external files waiting for initPage_ to be
   * called after the extensions info has been fetched.
   */
  private pageInitializedResolver_: PromiseResolver<void> =
      new PromiseResolver<void>();

  /**
   * The current page being shown. Default to null, and initPage_ will figure
   * out the initial page based on url.
   */
  private currentPage_: PageState|null = null;

  /**
   * The ID of the listener on |navigation|. Stored so that the
   * listener can be removed when this element is detached (happens in tests).
   */
  private navigationListener_: number|null = null;

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.addEventListener('load-error', this.onLoadError_);
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-start', this.onViewExitStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);

    const service = Service.getInstance();

    const onProfileStateChanged =
        (profileInfo: chrome.developerPrivate.ProfileInfo) => {
          this.isChildAccount_ = profileInfo.isChildAccount;
          this.incognitoAvailable_ = profileInfo.isIncognitoAvailable;
          this.devModeControlledByPolicy =
              profileInfo.isDeveloperModeControlledByPolicy;
          this.inDevMode = profileInfo.inDeveloperMode;
          this.canLoadUnpacked = profileInfo.canLoadUnpacked;
          this.isMv2DeprecationNoticeDismissed =
              profileInfo.isMv2DeprecationNoticeDismissed;
        };
    service.getProfileStateChangedTarget().addListener(onProfileStateChanged);
    service.getProfileConfiguration().then(onProfileStateChanged);

    service.getExtensionsInfo().then(extensionsAndApps => {
      this.initExtensionsAndApps_(extensionsAndApps);
      this.initPage_();

      service.getItemStateChangedTarget().addListener(
          this.onItemStateChanged_.bind(this));
    });
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('narrow_')) {
      const drawer = this.shadowRoot!.querySelector('cr-drawer');
      if (!this.narrow_ && drawer?.open) {
        drawer.close();
      }

      // TODO(crbug.com/c/1451985): Handle changing focus if focus is on the
      // sidebar or menu when it's about to disappear when `this.narrow_`
      // changes.
    }
  }

  override connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');
    // https://github.com/microsoft/TypeScript/issues/13569
    (document as any).fonts.load('bold 12px Roboto');

    this.navigationListener_ = navigation.addListener(newPage => {
      this.changePage_(newPage);
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.navigationListener_);
    assert(navigation.removeListener(this.navigationListener_));
    this.navigationListener_ = null;
  }

  /**
   * @return the promise of `pageInitializedResolver_` so tests can wait for the
   * page to be initialized.
   */
  whenPageInitializedForTest(): Promise<void> {
    return this.pageInitializedResolver_.promise;
  }

  /**
   * Initializes the page to reflect what's specified in the url so that if
   * the user visits chrome://extensions/?id=..., we land on the proper page.
   */
  private initPage_() {
    this.didInitPage_ = true;
    this.changePage_(navigation.getCurrentPage());
    this.pageInitializedResolver_.resolve();
  }

  protected onNarrowChanged_(e: CustomEvent<{value: boolean}>) {
    this.narrow_ = e.detail.value;
  }

  private onItemStateChanged_(eventData: chrome.developerPrivate.EventData) {
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
      case EventType.SERVICE_WORKER_STARTED:
      case EventType.SERVICE_WORKER_STOPPED:
      case EventType.PINNED_ACTIONS_CHANGED:
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
        const currentIndex = this.getListFromId_(listId).findIndex(
            (item: chrome.developerPrivate.ExtensionInfo) =>
                item.id === eventData.extensionInfo!.id);

        if (currentIndex >= 0) {
          this.updateItem_(listId, currentIndex, eventData.extensionInfo);
        } else {
          this.addItem_(listId, eventData.extensionInfo);
        }
        break;
      case EventType.UNINSTALLED:
        this.removeItem_(eventData.item_id);
        break;
      case EventType.CONFIGURATION_CHANGED:
        const index = this.getIndexInList_('extensions_', eventData.item_id);
        this.updateItem_(
            'extensions_', index,
            Object.assign({}, this.getData_(eventData.item_id), {
              didAcknowledgeMV2DeprecationNotice:
                  eventData.extensionInfo?.didAcknowledgeMV2DeprecationNotice,
              safetyCheckText: eventData.extensionInfo?.safetyCheckText,
            }));
        break;
      default:
        assertNotReached();
    }
  }

  protected onFilterChanged_(event: CustomEvent<string>) {
    if (this.currentPage_!.page !== Page.LIST) {
      navigation.navigateTo({page: Page.LIST});
    }
    this.filter = event.detail;
  }

  protected onMenuButtonClick_() {
    this.showDrawer_ = true;
    setTimeout(() => {
      this.shadowRoot!.querySelector('cr-drawer')!.openDrawer();
    }, 0);
  }

  /**
   * @return The ID of the list that the item belongs in.
   */
  private getListId_(item: chrome.developerPrivate.ExtensionInfo): string {
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
      default:
        assertNotReached();
    }
  }

  /**
   * @param listId The list to look for the item in.
   * @param itemId The id of the item to look for.
   * @return The index of the item in the list, or -1 if not found.
   */
  private getIndexInList_(listId: string, itemId: string): number {
    return this.getListFromId_(listId).findIndex(function(
        item: chrome.developerPrivate.ExtensionInfo) {
      return item.id === itemId;
    });
  }

  private getData_(id: string): chrome.developerPrivate.ExtensionInfo
      |undefined {
    return this.extensions_[this.getIndexInList_('extensions_', id)] ||
        this.apps_[this.getIndexInList_('apps_', id)];
  }

  /**
   * Categorizes |extensionsAndApps| to apps and extensions and initializes
   * those lists.
   */
  private initExtensionsAndApps_(extensionsAndApps:
                                     chrome.developerPrivate.ExtensionInfo[]) {
    extensionsAndApps.sort(compareExtensions);
    const apps: chrome.developerPrivate.ExtensionInfo[] = [];
    const extensions: chrome.developerPrivate.ExtensionInfo[] = [];
    for (const i of extensionsAndApps) {
      const list = this.getListId_(i) === 'apps_' ? apps : extensions;
      list.push(i);
    }

    this.apps_ = apps;
    this.extensions_ = extensions;
  }

  /**
   * Creates and adds a new extensions-item element to the list, inserting it
   * into its sorted position in the relevant section.
   * @param item The extension the new element is representing.
   */
  private addItem_(
      listId: string, item: chrome.developerPrivate.ExtensionInfo) {
    // We should never try and add an existing item.
    assert(this.getIndexInList_(listId, item.id) === -1);
    const list = this.getListFromId_(listId);
    let insertBeforeChild =
        list.findIndex(function(listEl: chrome.developerPrivate.ExtensionInfo) {
          return compareExtensions(listEl, item) > 0;
        });
    if (insertBeforeChild === -1) {
      insertBeforeChild = list.length;
    }
    this.updateList_(listId, insertBeforeChild, 0, item);
  }

  private getListFromId_(listId: string):
      chrome.developerPrivate.ExtensionInfo[] {
    assert(listId === 'extensions_' || listId === 'apps_');
    return listId === 'extensions_' ? this.extensions_ : this.apps_;
  }

  // Intentionally creating a new array reference to notify the Lit item-list
  // child via data bindings.
  private updateList_(
      listId: string, index: number, removeCount: number,
      newItem?: chrome.developerPrivate.ExtensionInfo) {
    const list = this.getListFromId_(listId);
    if (newItem) {
      list.splice(index, removeCount, newItem);
    } else {
      list.splice(index, removeCount);
    }
    listId === 'extensions_' ? this.extensions_ = list.slice() :
                               this.apps_ = list.slice();
  }

  /**
   * @param item The data for the item to update.
   */
  private updateItem_(
      listId: string, index: number,
      item: chrome.developerPrivate.ExtensionInfo) {
    // We should never try and update a non-existent item.
    assert(index >= 0);
    this.updateList_(listId, index, 1, item);

    // Update the subpage if it is open and displaying the item. If it's not
    // open, we don't update the data even if it's displaying that item. We'll
    // set the item correctly before opening the page. It's a little weird
    // that the DOM will have stale data, but there's no point in causing the
    // extra work.
    if (this.detailViewItem_ && this.detailViewItem_.id === item.id &&
        this.currentPage_!.page === Page.DETAILS) {
      this.detailViewItem_ = item;
    } else if (
        this.errorPageItem_ && this.errorPageItem_.id === item.id &&
        this.currentPage_!.page === Page.ERRORS) {
      this.errorPageItem_ = item;
    } else if (
        this.activityLogItem_ && this.activityLogItem_.id === item.id &&
        this.currentPage_!.page === Page.ACTIVITY_LOG) {
      this.activityLogItem_ = item;
    }
  }

  // When an item is removed while on the 'item list' page, move focus to the
  // next item in the list with `listId` if available. If no items are in that
  // list, focus to the search bar as a fallback.
  // This is a fix for crbug.com/1416324 which causes focus to linger on a
  // deleted element, which is then read by the screen reader.
  private focusAfterItemRemoved_(listId: string, index: number) {
    // A timeout is used so elements are focused after the DOM is updated.
    setTimeout(() => {
      const list = this.getListFromId_(listId);
      if (list.length) {
        const focusIndex = Math.min(list.length - 1, index);
        const itemToFocusId = list[focusIndex]!.id;

        // In the rare case where the item cannot be focused despite existing,
        // focus the search bar.
        if (!this.$['items-list'].focusItemButton(itemToFocusId)) {
          this.$.toolbar.focusSearchInput();
        }
      } else {
        this.$.toolbar.focusSearchInput();
      }
    }, 0);
  }

  /**
   * @param itemId The id of item to remove.
   */
  private removeItem_(itemId: string) {
    // Search for the item to be deleted in `extensions_`.
    let listId = 'extensions_';
    let index = this.getIndexInList_(listId, itemId);
    if (index === -1) {
      // If not in `extensions_` it must be in `apps_`.
      listId = 'apps_';
      index = this.getIndexInList_(listId, itemId);
    }

    // We should never try and remove a non-existent item.
    assert(index >= 0);
    this.updateList_(listId, index, 1);
    if (this.currentPage_!.page === Page.LIST) {
      // Wait for the items list to be updated with the new value before trying
      // to focus an item.
      this.$['items-list'].updateComplete.then(() => {
        this.focusAfterItemRemoved_(listId, index);
      });
    } else if (
        (this.currentPage_!.page === Page.ACTIVITY_LOG ||
         this.currentPage_!.page === Page.DETAILS ||
         this.currentPage_!.page === Page.ERRORS) &&
        this.currentPage_!.extensionId === itemId) {
      // Leave the details page (the 'item list' page is a fine choice).
      navigation.replaceWith({page: Page.LIST});
    }
  }

  private onLoadError_(
      e: CustomEvent<Error|chrome.developerPrivate.LoadError>) {
    this.showLoadErrorDialog_ = true;
    setTimeout(() => {
      const dialog = this.shadowRoot!.querySelector('extensions-load-error')!;
      dialog.loadError = e.detail;
      dialog.show();
    }, 0);
  }

  /**
   * Changes the active page selection.
   */
  private changePage_(newPage: PageState) {
    this.onCloseDrawer_();

    const optionsDialog =
        this.shadowRoot!.querySelector('extensions-options-dialog');
    if (optionsDialog && optionsDialog.open) {
      this.showOptionsDialog_ = false;
    }

    const fromPage = this.currentPage_ ? this.currentPage_.page : null;
    const toPage = newPage.page;
    let data: chrome.developerPrivate.ExtensionInfo|undefined;
    let activityLogPlaceholder;
    if (toPage === Page.LIST) {
      // Dismiss menu notifications for extensions module of Safety Hub.
      this.delegate.dismissSafetyHubExtensionsMenuNotification();
    }
    if (newPage.extensionId) {
      data = this.getData_(newPage.extensionId);
      if (!data) {
        // Allow the user to navigate to the activity log page even if the
        // extension ID is not valid. This enables the use case of seeing an
        // extension's install-time activities by navigating to an extension's
        // activity log page, then installing the extension.
        if (this.showActivityLog && toPage === Page.ACTIVITY_LOG) {
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

    if (toPage === Page.DETAILS) {
      this.detailViewItem_ = data;
    } else if (toPage === Page.ERRORS) {
      this.errorPageItem_ = data;
    } else if (toPage === Page.ACTIVITY_LOG) {
      if (!this.showActivityLog) {
        // Redirect back to the details page if we try to view the
        // activity log of an extension but the flag is not set.
        navigation.replaceWith(
            {page: Page.DETAILS, extensionId: newPage.extensionId});
        return;
      }

      this.activityLogItem_ = data || activityLogPlaceholder;
    } else if (
        (toPage === Page.SITE_PERMISSIONS ||
         toPage === Page.SITE_PERMISSIONS_ALL_SITES) &&
        !this.enableEnhancedSiteControls) {
      // Redirect back to the main page if we try to view the new site
      // permissions page but the flag is not set.
      navigation.replaceWith({page: Page.LIST});
      return;
    }

    if (fromPage !== toPage) {
      this.$.viewManager.switchView(toPage, 'no-animation', 'no-animation');
    }

    if (newPage.subpage) {
      assert(newPage.subpage === Dialog.OPTIONS);
      assert(newPage.extensionId);
      this.showOptionsDialog_ = true;
      setTimeout(() => {
        this.shadowRoot!.querySelector('extensions-options-dialog')!.show(
            data!,
        );
      }, 0);
    }

    document.title = toPage === Page.DETAILS ?
        `${loadTimeData.getString('title')} - ${this.detailViewItem_!.name}` :
        loadTimeData.getString('title');
    this.currentPage_ = newPage;
  }

  /**
   * This method detaches the drawer dialog completely. Should only be
   * triggered by the dialog's 'close' event.
   */
  protected onDrawerClose_() {
    this.showDrawer_ = false;
  }

  /**
   * This method animates the closing of the drawer.
   */
  protected onCloseDrawer_() {
    const drawer = this.shadowRoot!.querySelector('cr-drawer');
    if (drawer && drawer.open) {
      drawer.close();
    }
  }

  protected onLoadErrorDialogClose_() {
    this.showLoadErrorDialog_ = false;
  }

  protected onOptionsDialogClose_() {
    this.showOptionsDialog_ = false;
    this.shadowRoot!.querySelector(
                        'extensions-detail-view')!.focusOptionsButton();
  }

  private onViewEnterStart_() {
    this.fromActivityLog_ = false;
  }

  private onViewExitStart_(e: Event) {
    const viewType = (e.composedPath()[0] as HTMLElement).tagName;
    this.fromActivityLog_ = viewType === 'EXTENSIONS-ACTIVITY-LOG';
  }

  private onViewExitFinish_(e: Event) {
    const viewType = (e.composedPath()[0] as HTMLElement).tagName;
    if (viewType === 'EXTENSIONS-ITEM-LIST' ||
        viewType === 'EXTENSIONS-KEYBOARD-SHORTCUTS' ||
        viewType === 'EXTENSIONS-ACTIVITY-LOG' ||
        viewType === 'EXTENSIONS-SITE-PERMISSIONS' ||
        viewType === 'EXTENSIONS-SITE-PERMISSIONS-BY-SITE') {
      return;
    }

    const extensionId =
        (e.composedPath()[0] as ExtensionsDetailViewElement).data.id;
    const list = this.shadowRoot!.querySelector('extensions-item-list')!;
    const button = viewType === 'EXTENSIONS-DETAIL-VIEW' ?
        list.getDetailsButton(extensionId) :
        list.getErrorsButton(extensionId);

    // The button will not exist, when returning from a details page
    // because the corresponding extension/app was deleted.
    if (button) {
      button.focus();
    }
  }

  protected onShowInstallWarnings_(e: CustomEvent<string[]>) {
    // Leverage Polymer data bindings instead of just assigning the
    // installWarnings on the dialog since the dialog hasn't been stamped
    // in the DOM yet.
    this.installWarnings_ = e.detail;
    this.showInstallWarningsDialog_ = true;
  }

  protected onInstallWarningsDialogClose_() {
    this.installWarnings_ = null;
    this.showInstallWarningsDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-manager': ExtensionsManagerElement;
  }
}

customElements.define(ExtensionsManagerElement.is, ExtensionsManagerElement);
