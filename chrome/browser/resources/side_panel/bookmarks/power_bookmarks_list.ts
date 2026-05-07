// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './icons.html.js';
import './power_bookmark_row.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_heading.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_icons.html.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';

import type {SpEmptyStateElement} from '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import type {PriceTrackingBrowserProxy} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PriceTrackingBrowserProxyImpl} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {IronListElement} from '//resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, Debouncer, PolymerElement, timeOut} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource, SortOrder, ViewType} from './bookmarks.mojom-webui.js';
import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {KeyArrowNavigationService} from './keyboard_arrow_navigation_service.js';
import {BOOKMARK_ROW_LOAD_EVENT} from './power_bookmark_row.js';
import type {PowerBookmarksDragDelegate} from './power_bookmarks_drag_manager.js';
import {PowerBookmarksDragManager} from './power_bookmarks_drag_manager.js';
import {getTemplate} from './power_bookmarks_list.html.js';
import type {Label} from './power_bookmarks_service.js';
import {editingDisabledByPolicy, PowerBookmarksService} from './power_bookmarks_service.js';
import {getFolderLabel} from './power_bookmarks_utils.js';

function getBookmarkName(bookmark: BookmarksTreeNode): string {
  return bookmark.title || bookmark.url || '';
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// BookmarksSidePanelSearchCTREvent in tools/metrics/histograms/enums.xml.
export enum SearchAction {
  SHOWN = 0,
  SEARCHED = 1,

  // Must be last.
  COUNT = 2,
}

export interface SortOption {
  sortOrder: SortOrder;
  label: string;
  lowerLabel: string;
}

export interface PowerBookmarksListElement {
  $: {
    bookmarks: HTMLElement,
    folderEmptyState: SpEmptyStateElement,
    heading: HTMLElement,
    scroller: HTMLElement,
  };
}

interface ListSectionVisibility {
  heading?: boolean;
  filterHeadings?: boolean;
  folderEmptyState?: boolean;
  newFolderButton?: boolean;
  bookmarksList?: boolean;
}

export class PowerBookmarksListElement extends PolymerElement implements
    PowerBookmarksDragDelegate {
  static get is() {
    return 'power-bookmarks-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      displayLists_: {
        type: Array,
        value: () => [],
      },

      compact_: {
        type: Boolean,
        value: () => loadTimeData.getInteger('viewType') === 0,
        observer: 'updateListScrollOffset_',
      },

      contextMenuBookmark: Object,

      activeFolderPath: {
        type: Array,
        value: () => [],
        notify: true,
      },

      imageUrls_: {
        type: Object,
        value: () => {
          return {};
        },
      },

      labels: {
        type: Array,
        value: () => [],
      },

      activeSortType: Object,

      activeSortIndex: Number,

      editing: {
        type: Boolean,
        value: false,
      },

      selectedBookmarks: {
        type: Object,
        value: () => {
          return {};
        },
      },

      renamingId_: {
        type: String,
        value: '',
      },

      bookmarksTreeViewEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('bookmarksTreeViewEnabled'),
      },

      hasLoadedData_: {
        type: Boolean,
        value: false,
      },

      searchQuery: String,
      shoppingCollectionFolderId_: String,

      updatedElementIds_: {
        type: Array,
        value: () => [],
      },

      hasSomeActiveFilter: {
        type: Boolean,
        value: false,
      },

      hasShownBookmarks: {
        type: Boolean,
        value: false,
        computed: 'computeHasShownBookmarks_(displayLists_.*)',
        notify: true,
      },

      canDrag_: {
        type: Boolean,
        value: true,
        computed: 'computeCanDrag_(editing, renamingId_, hasSomeActiveFilter)',
        observer: 'onCanDragChange_',
      },

      hasActiveDrag_: {
        type: Boolean,
        value: false,
      },

      sectionVisibility_: {
        type: Object,
        computed: 'computeSectionVisibility_(hasLoadedData_,' +
            'activeFolderPath.length, hasShownBookmarks,' +
            'labels.length, hasSomeActiveFilter)',
      },

      hasFolders_: {
        type: Boolean,
        computed: 'computeHasFolders_(displayLists_.*)',
        reflect: true,
      },
    };
  }

  static get observers() {
    return [
      'updateDisplayLists_(activeFolderPath.splices, labels.*, ' +
          'activeSortType, searchQuery)',
    ];
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private priceTrackingProxy_: PriceTrackingBrowserProxy =
      PriceTrackingBrowserProxyImpl.getInstance();
  private bookmarksService_: PowerBookmarksService =
      PowerBookmarksService.getInstance();
  private keyArrowNavigationService_: KeyArrowNavigationService;
  private bookmarksDragManager_: PowerBookmarksDragManager;
  private focusOutlineManager_: FocusOutlineManager;
  private shownBookmarksResizeObserver_?: ResizeObserver;
  private recordCountMetricsOnNextUpdate_: boolean = false;
  private rebuildNavigationElementsDebouncer_: Debouncer|null = null;

  declare activeFolderPath: BookmarksTreeNode[];
  declare activeSortIndex: number;
  declare activeSortType: SortOption;
  declare contextMenuBookmark: BookmarksTreeNode|undefined;
  declare editing: boolean;
  declare hasSomeActiveFilter: boolean;
  declare hasShownBookmarks: boolean;
  declare labels: Label[];
  declare searchQuery: string|undefined;

  declare private compact_: boolean;
  declare private displayLists_: BookmarksTreeNode[][];
  declare private imageUrls_: {[key: string]: string};
  declare private selectedBookmarks: {[key: string]: boolean};
  declare private hasLoadedData_: boolean;
  declare private canDrag_: boolean;
  declare private hasActiveDrag_: boolean;
  declare private sectionVisibility_: ListSectionVisibility;
  declare private hasFolders_: boolean;
  declare private renamingId_: string;
  declare private shoppingCollectionFolderId_: string;
  declare private updatedElementIds_: string[];
  declare private bookmarksTreeViewEnabled_: boolean;

  constructor() {
    super();
    const keyArrowNavigationService =
        new KeyArrowNavigationService(this, 'power-bookmark-row:not([hidden])');
    KeyArrowNavigationService.setInstance(keyArrowNavigationService);
    this.keyArrowNavigationService_ = keyArrowNavigationService;
    this.bookmarksDragManager_ = new PowerBookmarksDragManager(this);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);

    this.shownBookmarksResizeObserver_ =
        new ResizeObserver(this.onShownBookmarksResize_.bind(this));
    this.shownBookmarksResizeObserver_.observe(this.$.bookmarks);

    this.updateListScrollOffset_();

    this.bookmarksDragManager_.startObserving();
    this.updateShoppingCollectionFolderId_();
    this.recordMetricsOnConnected_();
    this.keyArrowNavigationService_.startListening();

    this.addEventListener(BOOKMARK_ROW_LOAD_EVENT, () => {
      this.rebuildNavigationElements_();
    });
  }

  override disconnectedCallback() {
    this.shownBookmarksResizeObserver_!.disconnect();
    this.shownBookmarksResizeObserver_ = undefined;

    this.bookmarksDragManager_.stopObserving();
    this.keyArrowNavigationService_.stopListening();
  }

  setImageUrl(bookmark: BookmarksTreeNode, url: string) {
    this.set(`imageUrls_.${bookmark.id}`, url);
    this.imageUrls_ = structuredClone(this.imageUrls_);
  }

  onBookmarksLoaded() {
    this.updateDisplayLists_();
    this.hasLoadedData_ = true;
  }

  onBookmarkChanged(id: string) {
    const bookmark = this.bookmarksService_.findBookmarkWithId(id)!;
     this.updatedElementIds_ = [bookmark.id];
    if (this.bookmarkShouldShow_(bookmark) ||
        this.bookmarkIsShowing_(bookmark)) {
      this.updateDisplayLists_();
    }
    this.notifyPathIfVisible_(id, 'title');
    this.notifyPathIfVisible_(id, 'url');
  }

  onBookmarkAdded(bookmark: BookmarksTreeNode, parent: BookmarksTreeNode) {
    if (this.bookmarkShouldShow_(bookmark)) {
      this.updateShoppingCollectionFolderId_();

      const scrollTop = this.$.scroller.scrollTop;
      this.updateDisplayLists_();
      if (bookmark.url) {
        getAnnouncerInstance().announce(loadTimeData.getStringF(
            'bookmarkCreated', getBookmarkName(bookmark)));
      } else {
        getAnnouncerInstance().announce(loadTimeData.getStringF(
            'bookmarkFolderCreated', getBookmarkName(bookmark)));
      }
      for (let i = 0; i < this.displayLists_.length; i++) {
        const indexInList = this.displayLists_[i].indexOf(bookmark);
        if (indexInList > -1) {
          const listElement = this.getDisplayListElement_(i);
          if (listElement &&
              (indexInList < listElement.firstVisibleIndex ||
               indexInList > listElement.lastVisibleIndex)) {
            listElement.scrollToIndex(indexInList);
          } else {
            afterNextRender(this, () => {
              this.$.scroller.scrollTop = scrollTop;
            });
          }
          break;
        }
      }
    }
    this.updatedElementIds_ = [bookmark.id];
    this.notifyPathIfVisible_(parent.id, 'children');
  }

  onBookmarkMoved(
      bookmark: BookmarksTreeNode, oldParent: BookmarksTreeNode,
      newParent: BookmarksTreeNode) {
    const shouldShow = this.bookmarkShouldShow_(bookmark);
    const isShowing = this.bookmarkIsShowing_(bookmark);
    if (oldParent === newParent && shouldShow) {
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkReordered', getBookmarkName(bookmark)));
    } else if (
        (shouldShow !== isShowing) ||
        (shouldShow && this.hasSomeActiveFilter)) {
      const scrollTop = this.$.scroller.scrollTop;
      this.updateDisplayLists_();
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(bookmark),
          getBookmarkName(newParent)));
      afterNextRender(this, () => {
        this.$.scroller.scrollTop = scrollTop;
      });
    }
    this.updatedElementIds_ = [newParent.id, oldParent.id];
    // If the new parent folder is visible, notify to ensure its displayed
    // child count is updated.
    this.notifyPathIfVisible_(newParent.id, 'children');
    this.notifyPathIfVisible_(oldParent.id, 'children');
    // If compact and tree view is active, we must resize open folders
    if (this.bookmarksTreeViewEnabled_ && this.compact_) {
      this.notifyBookmarksListResize_();
    }
  }

  onBookmarkRemoved(bookmark: BookmarksTreeNode) {
    const scrollTop = this.$.scroller.scrollTop;
    this.updateDisplayLists_();
    const isShown = this.bookmarkIsShowing_(bookmark);
    if (isShown) {
      this.removeNodeFromDisplayLists_(bookmark.id);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkDeleted', getBookmarkName(bookmark)));
      afterNextRender(this, () => {
        this.$.scroller.scrollTop = scrollTop;
      });
    }

    if (this.shoppingCollectionFolderId_ === bookmark.id) {
      this.shoppingCollectionFolderId_ = '';
    }
    this.updatedElementIds_ = [bookmark.parentId];

    // If the parent folder is visible, notify to ensure its displayed
    // child count is updated.
    this.notifyPathIfVisible_(bookmark.parentId, 'children');
    this.rebuildNavigationElements_();
  }

  /** PowerBookmarksDragDelegate */
  getFallbackBookmark(): BookmarksTreeNode {
    // Returning other bookmarks folder in tree view allow moving bookmarks to
    // the root folder
    if (this.bookmarksTreeViewEnabled_ && this.compact_) {
      return this.bookmarksService_.findBookmarkWithId(
          loadTimeData.getString('otherBookmarksId'))!;
    }

    return this.getParentFolder();
  }

  /** PowerBookmarksDragDelegate */
  getFallbackDropTargetElement(): HTMLElement {
    return this;
  }

  /** PowerBookmarksDragDelegate */
  onFinishDrop(dropTarget: BookmarksTreeNode): void {
    this.updateDisplayLists_();
    this.focusBookmark_(dropTarget.id);

    // Show the focus state immediately after dropping a bookmark to indicate
    // where the bookmark was moved to, and remove the state immediately after
    // the next mouse event.
    this.focusOutlineManager_.visible = true;
    document.addEventListener('mousedown', () => {
      this.focusOutlineManager_.visible = false;
    }, {once: true});
  }

  /** PowerBookmarksDragDelegate */
  setHasActiveDrag(hasActiveDrag: boolean): void {
    this.hasActiveDrag_ = hasActiveDrag;
  }

  clickBookmarkRowForTests(bookmark: BookmarksTreeNode) {
    const event = new CustomEvent('row-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: bookmark,
        event: new MouseEvent('row-clicked'),
      },
    });
    this.onRowClicked_(event);
  }

  setRenamingIdForTests(id: string) {
    this.renamingId_ = id;
  }

  /**
   * Returns the KeyboardNavigationService instance for testing.
   */
  getKeyboardNavigationServiceforTesting() {
    return this.keyArrowNavigationService_;
  }

  getDragManagerForTesting() {
    return this.bookmarksDragManager_;
  }

  flushNavigationElementsDebouncerForTesting() {
    if (this.rebuildNavigationElementsDebouncer_) {
      this.rebuildNavigationElementsDebouncer_.flush();
    }
  }

  private notifyPathIfVisible_(id: string, key: string) {
    for (let i = 0; i < this.displayLists_.length; i++) {
      const listIndex = this.displayLists_[i].findIndex(b => b.id === id);
      if (listIndex > -1) {
        this.notifyPath(`displayLists_.${i}.${listIndex}.${key}`);
        return;
      }
    }
  }

  private computeHasFolders_(): boolean {
    if (!this.displayLists_ || this.displayLists_.length === 0) {
      return false;
    }
    return this.displayLists_.some(
        list => list.some(bookmark => !!bookmark.children),
    );
  }

  private computeCanDrag_(): boolean {
    return !this.editing && !this.renamingId_ && !this.hasSomeActiveFilter;
  }

  private focusBookmark_(id: string) {
    const bookmarkElement =
        this.shadowRoot!.querySelector<HTMLElement>(`#bookmark-${id}`);
    if (bookmarkElement) {
      bookmarkElement.focus();
    }
  }

  private bookmarkIsShowing_(bookmark: BookmarksTreeNode): boolean {
    return this.displayLists_.some(
        list => list.some(item => item.id === bookmark.id));
  }

  private removeNodeFromDisplayLists_(nodeId: string) {
    for (let listIndex = 0; listIndex < this.displayLists_.length;
         listIndex++) {
      const itemIndex =
          this.displayLists_[listIndex].findIndex(b => b.id === nodeId);
      if (itemIndex > -1) {
        this.splice(`displayLists_.${listIndex}`, itemIndex, 1);
      }
    }
  }

  /**
   * Returns true if the given node is either the current active folder or a
   * root folder that isn't shown itself while the all bookmarks list is shown.
   */
  private visibleParent_(parent: BookmarksTreeNode): boolean {
    const activeFolder = this.getActiveFolder();
    return (!activeFolder &&
            parent.parentId === loadTimeData.getString('rootBookmarkId') &&
            !this.bookmarkIsShowing_(parent)) ||
        parent === activeFolder;
  }

  private bookmarkShouldShow_(bookmark: BookmarksTreeNode): boolean {
    if (this.hasSomeActiveFilter) {
      return this.bookmarksService_.bookmarkMatchesSearchQueryAndLabels(
          bookmark, this.labels, this.searchQuery);
    }
    return this.visibleParent_(
        this.bookmarksService_.findBookmarkWithId(bookmark.parentId)!);
  }

  getActiveFolder(): BookmarksTreeNode|undefined {
    if (this.activeFolderPath.length) {
      return this.activeFolderPath[this.activeFolderPath.length - 1];
    }
    return undefined;
  }

  private getBackButtonLabel_(): string {
    const activeFolder = this.getActiveFolder();
    const parentFolder = this.bookmarksService_.findBookmarkWithId(
        activeFolder ? activeFolder.parentId : undefined);
    return loadTimeData.getStringF(
        'backButtonLabel', getFolderLabel(parentFolder));
  }

  private getBookmarksListRole_(): string {
    return this.editing ? 'listbox' : 'list';
  }

  private getViewButtonIcon_() {
    return this.compact_ ? 'bookmarks:compact-view' : 'bookmarks:visual-view';
  }

  private getViewButtonTooltip_() {
    return this.compact_ ? loadTimeData.getString('compactView') :
                           loadTimeData.getString('visualView');
  }

  private updateShoppingCollectionFolderId_(): void {
    this.priceTrackingProxy_.getShoppingCollectionBookmarkFolderId().then(
        res => {
          this.shoppingCollectionFolderId_ = res.collectionId.toString();
        });
  }

  private getActiveFolderLabel_(): string {
    return getFolderLabel(this.getActiveFolder());
  }

  /**
   * Update the lists of bookmarks and folders displayed to the user.
   */
  private updateDisplayLists_() {
    const activeFolder = this.getActiveFolder();
    const primaryList = this.bookmarksService_.filterBookmarks(
        activeFolder, this.activeSortType.sortOrder, this.searchQuery,
        this.labels);
    if (this.hasSomeActiveFilter && !!activeFolder) {
      const secondaryList = this.bookmarksService_.filterBookmarks(
          undefined, this.activeSortType.sortOrder, this.searchQuery,
          this.labels, activeFolder);
      this.displayLists_ = [primaryList, secondaryList];
    } else {
      this.displayLists_ = [primaryList];
    }
    this.displayLists_.forEach(
        list => this.bookmarksService_.refreshDataForBookmarks(list));
    this.updateListScrollOffset_();

    if (this.recordCountMetricsOnNextUpdate_ && this.hasLoadedData_) {
      this.recordBookmarkCountMetrics_();
    }

    // After the lists are updated and all children updates are complete,
    // notify iron-list to resize.
    afterNextRender(this, () => {
      const children =
          [...this.shadowRoot!.querySelectorAll('power-bookmark-row')];
      if (children.length > 0) {
        Promise.all(children.map(el => el.updateComplete))
            .then(() => {
              this.notifyBookmarksListResize_();

              // Make sure the keyboard navigation tree is rebuilt whenever the
              // iron-list is updated.
              this.rebuildNavigationElements_();
            });
      }
    });
  }

  private updateListScrollOffset_() {
    // Set scrollOffset so the iron-list scrolling accounts for the space the
    // other scrolling UI elements take.
    afterNextRender(this, () => {
      const primaryList = this.getDisplayListElement_(0);
      const secondaryList = this.getDisplayListElement_(1);
      const bookmarksOffsetTop = this.$.bookmarks.offsetTop;
      if (primaryList) {
        primaryList.scrollOffset = primaryList.offsetTop - bookmarksOffsetTop;
      }
      if (secondaryList) {
        secondaryList.scrollOffset =
            secondaryList.offsetTop - bookmarksOffsetTop;
      }
    });
  }

  private onCanDragChange_() {
    if (this.canDrag_) {
      this.bookmarksDragManager_.startObserving();
    } else {
      this.bookmarksDragManager_.stopObserving();
    }
  }

  private recordMetricsOnConnected_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.SortTypeShown', this.activeSortType.sortOrder,
        SortOrder.kCount);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.ViewTypeShown',
        this.compact_ ? ViewType.kCompact : ViewType.kExpanded,
        ViewType.kCount);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.Search.CTR', SearchAction.SHOWN,
        SearchAction.COUNT);
    this.recordCountMetricsOnNextUpdate_ = true;
  }

  private rebuildNavigationElements_() {
    this.rebuildNavigationElementsDebouncer_ = Debouncer.debounce(
        this.rebuildNavigationElementsDebouncer_, timeOut.after(1), () => {
          this.keyArrowNavigationService_.rebuildNavigationElements();
          if (this.recordCountMetricsOnNextUpdate_) {
            this.recordBookmarkCountMetricsInternal_();
            this.recordCountMetricsOnNextUpdate_ = false;
          }
        });
  }

  private recordBookmarkCountMetrics_() {
    if (this.bookmarksTreeViewEnabled_) {
      this.recordCountMetricsOnNextUpdate_ = true;
      this.rebuildNavigationElements_();
    } else {
      this.recordBookmarkCountMetricsInternal_();
      this.recordCountMetricsOnNextUpdate_ = false;
    }
  }

  private recordBookmarkCountMetricsInternal_() {
    const count = this.bookmarksTreeViewEnabled_ ?
        this.keyArrowNavigationService_.getElementCount() :
        this.displayLists_.reduce((prev, curr) => prev + curr.length, 0);
    const metricName = `PowerBookmarks.SidePanel${
        this.hasSomeActiveFilter ? '.SearchOrFilter' : ''}.BookmarksShown`;
    chrome.metricsPrivate.recordMediumCount(metricName, count);
  }

  private onRowToggled_(_event: CustomEvent<{
    bookmark: BookmarksTreeNode,
    expanded: boolean,
    event: MouseEvent,
  }>) {
    this.notifyBookmarksListResize_();
    this.recordBookmarkCountMetrics_();
  }
  /**
   * Invoked when the user clicks a power bookmarks row. This will either
   * display children in the case of a folder row, or open the URL in the case
   * of a bookmark row.
   */
  private onRowClicked_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (!this.editing) {
      if (event.detail.bookmark.children) {
        this.recordCountMetricsOnNextUpdate_ = true;
        this.push('activeFolderPath', event.detail.bookmark);
        this.dispatchEvent(
            new CustomEvent('clear-search', {bubbles: true, composed: true}));
        afterNextRender(this, () => {
          for (let i = 0; i < this.displayLists_.length; i++) {
            if (this.displayLists_[i].length > 0) {
              this.getDisplayListElement_(i)!.focusItem(0);
              break;
            }
          }
        });
      } else {
        this.bookmarksApi_.openBookmark(
            event.detail.bookmark.id, this.activeFolderPath.length, {
              middleButton: event.detail.event.button === 1,
              altKey: event.detail.event.altKey,
              ctrlKey: event.detail.event.ctrlKey,
              metaKey: event.detail.event.metaKey,
              shiftKey: event.detail.event.shiftKey,
            },
            ActionSource.kBookmark);
      }
    }
    // Workaround for this issue, causing unexpected list scrolling when
    // refocusing the list after changing tabs:
    // https://github.com/PolymerElements/iron-list/issues/270
    if (event.target) {
      (event.target as HTMLElement).blur();
    }
  }

  private onRowSelectedChange_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, checked: boolean}>) {
    event.preventDefault();
    event.stopPropagation();
    const isSelected =
        Object.entries(this.selectedBookmarks)
            .find(([key, _val]) => key === event.detail.bookmark.id)
            ?.[1] ??
        false;
    if ((event.detail.checked && !isSelected) ||
        (!event.detail.checked && isSelected)) {
      this.dispatchEvent(new CustomEvent('row-selected-change', {
        bubbles: true,
        composed: true,
        detail: {
          id: event.detail.bookmark.id,
          checked: event.detail.checked,
        },
      }));
    }
  }

  private onRename_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, value: string|null}>) {
    const newName = event.detail.value;
    if (newName != null) {
      this.bookmarksApi_.renameBookmark(event.detail.bookmark.id, newName);
    }
    this.renamingId_ = '';
  }

  private getDisplayListElement_(index: number): IronListElement|null {
    return this.shadowRoot!.querySelector<IronListElement>(
        `#shownBookmarksIronList${index}`);
  }

  private notifyBookmarksListResize_() {
    for (let i = 0; i < this.displayLists_.length; i++) {
      const displayListElement = this.getDisplayListElement_(i);
      // When switching between filtered and non-filtered views, the list of
      // display elements might become briefly out of sync with
      // `this.displayLists_` so check that it exists.
      if (displayListElement) {
        displayListElement.notifyResize();
      }
    }
  }

  private getFilterHeading_(index: number) {
    if (index === 0) {
      return loadTimeData.getStringF(
          'primaryFilterHeading', this.getActiveFolderLabel_());
    }
    return loadTimeData.getString('secondaryFilterHeading');
  }

  private getSelectedDescription_() {
    return loadTimeData.getStringF(
        'selectedBookmarkCount', this.getSelectedBookmarksLength_());
  }

  private getSelectedBookmarksList_(): BookmarksTreeNode[] {
    const selectedEntries =
        Object.entries(this.selectedBookmarks).filter(([_id,
                                                        selected]) => selected);
    const selectedIds = selectedEntries.map(([id, _selected]) => id);
    return selectedIds
        .map((id) => this.bookmarksService_.findBookmarkWithId(id)!)
        .filter(b => !!b);
  }

  private getSelectedBookmarksLength_(): number {
    return Object.values(this.selectedBookmarks)
        .filter((selected) => selected)
        .length;
  }

  /**
   * Toggles the given label between active and inactive.
   */
  /**
   * Moves the displayed folders up one level when the back button is clicked.
   */
  private onBackClicked_() {
    this.recordCountMetricsOnNextUpdate_ = true;
    this.pop('activeFolderPath');
  }

  private shouldHideBackButton_(): boolean {
    return !this.activeFolderPath.length;
  }

  private onShowContextMenuClicked_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, event: MouseEvent}>) {
    this.dispatchEvent(new CustomEvent('show-context-menu', {
      bubbles: true,
      composed: true,
      detail: event.detail,
    }));
  }

  getParentFolder(): BookmarksTreeNode {
    return this.getActiveFolder() ||
        this.bookmarksService_.findBookmarkWithId(
            loadTimeData.getString('otherBookmarksId'))!;
  }

  private onShowSortMenuClicked_(event: MouseEvent) {
    this.dispatchEvent(new CustomEvent('show-sort-menu', {
      bubbles: true,
      composed: true,
      detail: event,
    }));
  }

  private onAddNewFolderClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const newParent = this.getParentFolder();
    if (editingDisabledByPolicy([newParent])) {
      this.dispatchEvent(
          new CustomEvent('disabled-feature', {bubbles: true, composed: true}));
      return;
    }
    chrome.metricsPrivate.recordUserAction(
        'Bookmarks.FolderAddedFromSidePanel');
    this.bookmarksApi_
        .createFolder(newParent.id, loadTimeData.getString('newFolderTitle'))
        .then((result: {newFolderId: string}) => {
          this.renamingId_ = result.newFolderId;
        });
  }

  private onBulkEditClicked_(event: MouseEvent) {
    this.dispatchEvent(new CustomEvent('bulk-edit-click', {
      bubbles: true,
      composed: true,
      detail: event,
    }));
  }

  private onViewToggleClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.compact_ = !this.compact_;
    if (this.bookmarksTreeViewEnabled_ && this.compact_) {
      this.updateDisplayLists_();
    }
    this.notifyBookmarksListResize_();
    const viewType = this.compact_ ? ViewType.kCompact : ViewType.kExpanded;
    this.bookmarksApi_.setViewType(viewType);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.ViewTypeShown', viewType, ViewType.kCount);
  }

  private disableBackButton_(): boolean {
    return !this.activeFolderPath.length || this.editing;
  }

  private computeHasShownBookmarks_(): boolean {
    return this.displayLists_.some((list) => list.length > 0);
  }

  private computeSectionVisibility_(): ListSectionVisibility {
    if (!this.hasLoadedData_) {
      return {};
    }

    const hasActiveFolder = this.activeFolderPath.length > 0;
    const hasShownBookmarks = this.hasShownBookmarks;
    const hasSomeActiveFilter = this.hasSomeActiveFilter;

    return {
      heading: !hasSomeActiveFilter && (hasActiveFolder || hasShownBookmarks),
      filterHeadings: hasSomeActiveFilter,
      folderEmptyState:
          !hasShownBookmarks && !hasSomeActiveFilter && hasActiveFolder,
      newFolderButton: !hasSomeActiveFilter,
      bookmarksList: hasShownBookmarks,
    };
  }

  private onShownBookmarksResize_() {
    // The iron-lists of `displayLists_` are in a dynamically sized card.
    // Any time the size changes, let iron-list know so that iron-list can
    // properly adjust to its possibly new height.
    this.notifyBookmarksListResize_();

    this.dispatchEvent(new CustomEvent('has-scrollbars-changed', {
      bubbles: true,
      composed: true,
      detail: this.$.scroller.scrollHeight > this.$.scroller.offsetHeight,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list': PowerBookmarksListElement;
  }
}

customElements.define(PowerBookmarksListElement.is, PowerBookmarksListElement);
