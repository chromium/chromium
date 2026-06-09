// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './icons.html.js';
import './power_bookmark_row.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_heading.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import '//resources/cr_elements/icons.html.js';
import './power_bookmarks_add_folder_button.js';
import './power_bookmarks_list_header.js';

import type {SpEmptyStateElement} from '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import type {PriceTrackingBrowserProxy} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {PriceTrackingBrowserProxyImpl} from '//resources/cr_components/commerce/price_tracking_browser_proxy.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrLazyListElement} from '//resources/cr_elements/cr_lazy_list/cr_lazy_list.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {ActionSource, ViewType} from './bookmarks.mojom-webui.js';
import type {BookmarksTreeNode, SortOrder} from './bookmarks.mojom-webui.js';
import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {KeyArrowNavigationService} from './keyboard_arrow_navigation_service.js';
import {BOOKMARK_ROW_LOAD_EVENT} from './power_bookmark_row.js';
import type {PowerBookmarksDragDelegate} from './power_bookmarks_drag_manager.js';
import {PowerBookmarksDragManager} from './power_bookmarks_drag_manager.js';
import {getCss} from './power_bookmarks_list.css.js';
import {getHtml} from './power_bookmarks_list.html.js';
import type {Label} from './power_bookmarks_service.js';
import {editingDisabledByPolicy, PowerBookmarksService} from './power_bookmarks_service.js';
import {getFolderLabel} from './power_bookmarks_utils.js';

export interface DisplayItem {
  bookmark: BookmarksTreeNode;
  depth: number;
}

function getBookmarkName(bookmark: BookmarksTreeNode): string {
  return bookmark.title || bookmark.url || '';
}

import {SearchAction, recordFolderAdded, recordSearchCTR, recordViewType, recordBookmarksShown} from './power_bookmarks_metrics.js';

export interface PowerBookmarksListElement {
  $: {
    bookmarks: HTMLElement,
    folderEmptyState: SpEmptyStateElement,
    heading: HTMLElement,
    scroller: HTMLElement,
    list: CrLazyListElement<DisplayItem>,
  };
}

interface ListSectionVisibility {
  heading?: boolean;
  filterHeadings?: boolean;
  folderEmptyState?: boolean;
  newFolderButton?: boolean;
  bookmarksList?: boolean;
}

export class PowerBookmarksListElement extends CrLitElement implements
    PowerBookmarksDragDelegate {
  static get is() {
    return 'power-bookmarks-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      displayList_: {
        type: Array,
      },

      compact_: {
        type: Boolean,
      },

      contextMenuBookmark: {type: Object},

      activeFolderPath: {
        type: Array,
        notify: true,
      },

      imageUrls_: {
        type: Object,
      },

      labels: {
        type: Array,
      },

      sortOrder: {type: Number},

      activeSortIndex: {type: Number},

      editing: {
        type: Boolean,
      },

      selectedBookmarks: {
        type: Object,
      },

      renamingId: {
        type: String,
        notify: true,
      },

      hasLoadedData_: {
        type: Boolean,
      },

      searchQuery: {type: String},
      shoppingCollectionFolderId_: {type: String},

      updatedElementIds_: {
        type: Array,
      },

      hasSomeActiveFilter: {
        type: Boolean,
      },

      hasShownBookmarks: {
        type: Boolean,
        notify: true,
      },

      canDrag_: {
        type: Boolean,
      },

      hasActiveDrag_: {
        type: Boolean,
      },

      sectionVisibility_: {
        type: Object,
      },

      hasFolders_: {
        type: Boolean,
        reflect: true,
      },

      firstSecondaryIndex_: {
        type: Number,
      },

      scrollTarget_: {
        type: Object,
      },
    };
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
  private rebuildNavigationElementsTimerId_: number = -1;
  private expandedFolderIds_: Set<string> = new Set();

  accessor activeFolderPath: BookmarksTreeNode[] = [];
  protected accessor activeSortIndex: number;
  protected accessor sortOrder: SortOrder;
  accessor contextMenuBookmark: BookmarksTreeNode|undefined;
  accessor editing: boolean = false;
  accessor hasSomeActiveFilter: boolean = false;
  accessor hasShownBookmarks: boolean = false;
  accessor labels: Label[] = [];
  accessor renamingId: string = '';
  accessor searchQuery: string|undefined;
  protected accessor compact_: boolean =
      loadTimeData.getInteger('viewType') === 0;
  protected accessor displayList_: DisplayItem[] = [];
  protected accessor imageUrls_: {[key: string]: string} = {};
  protected accessor selectedBookmarks: {[key: string]: boolean} = {};
  protected accessor hasLoadedData_: boolean = false;
  protected accessor canDrag_: boolean = true;
  protected accessor hasActiveDrag_: boolean = false;
  protected accessor sectionVisibility_: ListSectionVisibility;
  protected accessor hasFolders_: boolean;
  protected accessor scrollTarget_: HTMLElement = document.documentElement;
  protected accessor shoppingCollectionFolderId_: string;
  protected accessor updatedElementIds_: string[] = [];
  protected accessor firstSecondaryIndex_: number = -1;

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
    super.disconnectedCallback();
    this.shownBookmarksResizeObserver_!.disconnect();
    this.shownBookmarksResizeObserver_ = undefined;

    this.bookmarksDragManager_.stopObserving();
    this.keyArrowNavigationService_.stopListening();

    if (this.rebuildNavigationElementsTimerId_ !== -1) {
      clearTimeout(this.rebuildNavigationElementsTimerId_);
      this.rebuildNavigationElementsTimerId_ = -1;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('searchQuery')) {
      this.onSearchChanged_();
    }

    let displayListChanged = false;
    if (changedPrivateProperties.has('activeFolderPath') ||
        changedPrivateProperties.has('labels') ||
        changedPrivateProperties.has('sortOrder') ||
        changedPrivateProperties.has('searchQuery') ||
        changedPrivateProperties.has('hasSomeActiveFilter')) {
      this.updateDisplayList_();
      displayListChanged = true;
    }

    if (changedPrivateProperties.has('editing') ||
        changedPrivateProperties.has('renamingId') ||
        changedPrivateProperties.has('hasSomeActiveFilter')) {
      this.canDrag_ = this.computeCanDrag_();
    }

    if (changedPrivateProperties.has('hasLoadedData_') || displayListChanged) {
      this.sectionVisibility_ = this.computeSectionVisibility_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.scrollTarget_ = this.$.scroller;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('compact_')) {
      this.updateListScrollOffset_();
    }

    if (changedPrivateProperties.has('canDrag_')) {
      this.onCanDragChange_();
    }
  }

  setImageUrl(bookmark: BookmarksTreeNode, url: string) {
    this.imageUrls_[bookmark.id] = url;
    this.imageUrls_ = {...this.imageUrls_};
  }

  onBookmarksLoaded() {
    this.updateDisplayList_();
    this.hasLoadedData_ = true;
  }

  onBookmarkChanged(id: string) {
    const bookmark = this.bookmarksService_.findBookmarkWithId(id)!;
     this.updatedElementIds_ = [bookmark.id];
    if (this.bookmarkShouldShow_(bookmark) ||
        this.bookmarkIsShowing_(bookmark)) {
      this.updateDisplayList_();
    }
  }

  private async scrollToBookmark_(bookmark: BookmarksTreeNode) {
    const index =
        this.displayList_.findIndex(item => item.bookmark.id === bookmark.id);
    if (index === -1) {
      return;
    }

    const element = await this.$.list.ensureItemRendered(index);
    element.scrollIntoView({block: 'nearest'});
  }

  onBookmarkAdded(bookmark: BookmarksTreeNode, parent: BookmarksTreeNode) {
    if (this.bookmarkShouldShow_(bookmark)) {
      this.updateShoppingCollectionFolderId_();

      this.updateDisplayList_();
      if (bookmark.url) {
        getAnnouncerInstance().announce(loadTimeData.getStringF(
            'bookmarkCreated', getBookmarkName(bookmark)));
      } else {
        getAnnouncerInstance().announce(loadTimeData.getStringF(
            'bookmarkFolderCreated', getBookmarkName(bookmark)));
      }
      this.scrollToBookmark_(bookmark);
    }
    this.updatedElementIds_ = [bookmark.id, parent.id];
  }

  onBookmarkMoved(
      bookmark: BookmarksTreeNode, oldParent: BookmarksTreeNode,
      newParent: BookmarksTreeNode) {
    const shouldShow = this.bookmarkShouldShow_(bookmark);
    const isShowing = this.bookmarkIsShowing_(bookmark);
    let noMetrics = false;
    if (oldParent === newParent && shouldShow) {
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkReordered', getBookmarkName(bookmark)));
      noMetrics = true;
    } else if (
        (shouldShow !== isShowing) ||
        (shouldShow && this.hasSomeActiveFilter)) {
      const scrollTop = this.$.scroller.scrollTop;
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(bookmark),
          getBookmarkName(newParent)));
      this.updateComplete.then(() => {
        this.$.scroller.scrollTop = scrollTop;
      });
    }
    this.updateDisplayList_(noMetrics);
    this.updatedElementIds_ = [newParent.id, oldParent.id];
    // If the new parent folder is visible, notify to ensure its displayed
    // child count is updated.
    // If compact, we must resize open folders
    if (this.compact_) {
      this.notifyBookmarksListResize_();
    }
  }

  onBookmarkRemoved(bookmark: BookmarksTreeNode) {
    getAnnouncerInstance().announce(
        loadTimeData.getStringF('bookmarkDeleted', getBookmarkName(bookmark)));

    const scrollTop = this.$.scroller.scrollTop;
    this.updateDisplayList_(/* noMetrics = */ true);
    this.updateComplete.then(() => {
      this.$.scroller.scrollTop = scrollTop;
    });

    if (this.shoppingCollectionFolderId_ === bookmark.id) {
      this.shoppingCollectionFolderId_ = '';
    }
    this.updatedElementIds_ = [bookmark.parentId];
  }

  /** PowerBookmarksDragDelegate */
  getFallbackBookmark(): BookmarksTreeNode {
    // Returning other bookmarks folder allows moving bookmarks to the root
    // folder
    if (this.compact_) {
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
    this.updateDisplayList_();
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
    if (this.rebuildNavigationElementsTimerId_ === -1) {
      return;
    }
    clearTimeout(this.rebuildNavigationElementsTimerId_);
    this.keyArrowNavigationService_.rebuildNavigationElements();
    this.rebuildNavigationElementsTimerId_ = -1;
  }

  private computeHasFolders_(): boolean {
    if (!this.displayList_ || this.displayList_.length === 0) {
      return false;
    }
    return this.displayList_.some(item => !item.bookmark.url);
  }

  private computeCanDrag_(): boolean {
    return !this.editing && !this.renamingId && !this.hasSomeActiveFilter;
  }

  private focusBookmark_(id: string) {
    const bookmarkElement =
        this.shadowRoot.querySelector<HTMLElement>(`#bookmark-${id}`);
    if (bookmarkElement) {
      bookmarkElement.focus();
    }
  }

  private bookmarkIsShowing_(bookmark: BookmarksTreeNode): boolean {
    return this.displayList_.some(item => item.bookmark.id === bookmark.id);
  }

  private removeNodeFromDisplayLists_(nodeId: string) {
    const itemIndex =
        this.displayList_.findIndex(item => item.bookmark.id === nodeId);
    if (itemIndex > -1) {
      this.displayList_.splice(itemIndex, 1);
      this.displayList_ = [...this.displayList_];
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

  protected getBookmarksListRole_(): string {
    return this.editing ? 'listbox' : 'list';
  }

  private updateShoppingCollectionFolderId_(): void {
    this.priceTrackingProxy_.getShoppingCollectionBookmarkFolderId().then(
        res => {
          this.shoppingCollectionFolderId_ = res.collectionId.toString();
        });
  }

  protected getActiveFolderLabel_(): string {
    return getFolderLabel(this.getActiveFolder());
  }

  /**
   * Recursively flattens the child bookmarks and sub-folders of an expanded
   * folder into the flat display list array, calculating appropriate
   * indentation depth.
   */
  private flattenFolder_(
      folder: BookmarksTreeNode, depth: number, list: DisplayItem[]) {
    if (!this.expandedFolderIds_.has(folder.id) || !folder.children) {
      return;
    }
    const sortedChildren = folder.children.slice();
    this.bookmarksService_.sortBookmarks(sortedChildren, this.activeSortIndex);
    for (const child of sortedChildren) {
      list.push({bookmark: child, depth: depth + 1});
      if (!child.url) {
        this.flattenFolder_(child, depth + 1, list);
      }
    }
  }

  private updateDisplayListObserver_() {
    this.updateDisplayList_();
  }

  /**
   * Update the lists of bookmarks and folders displayed to the user.
   */
  private updateDisplayList_(noMetrics: boolean = false) {
    const activeFolder = this.getActiveFolder();
    const primaryList = this.bookmarksService_.filterBookmarks(
        activeFolder, this.sortOrder, this.searchQuery, this.labels);
    this.bookmarksService_.refreshDataForBookmarks(primaryList);

    let secondaryList: BookmarksTreeNode[] = [];
    if (this.hasSomeActiveFilter && !!activeFolder) {
      secondaryList = this.bookmarksService_.filterBookmarks(
          undefined, this.sortOrder, this.searchQuery, this.labels,
          activeFolder);
      this.bookmarksService_.refreshDataForBookmarks(secondaryList);
    }

    const displayList: DisplayItem[] = [];
    for (const node of primaryList) {
      displayList.push({bookmark: node, depth: 0});
      if (!node.url) {
        this.flattenFolder_(node, 0, displayList);
      }
    }

    const firstSecondaryIndex = displayList.length;

    for (const node of secondaryList) {
      displayList.push({bookmark: node, depth: 0});
      if (!node.url) {
        this.flattenFolder_(node, 0, displayList);
      }
    }

    this.firstSecondaryIndex_ =
        secondaryList.length > 0 ? firstSecondaryIndex : -1;

    this.displayList_ = displayList;
    this.hasShownBookmarks = this.computeHasShownBookmarks_();
    this.hasFolders_ = this.computeHasFolders_();
    this.updateListScrollOffset_();

    // After the lists are updated and all children updates are complete,
    // notify cr-lazy-list to resize.
    this.updateComplete.then(async () => {
      // Allow time for child Lit elements to render.
      await new Promise(resolve => setTimeout(resolve, 0));
      this.notifyBookmarksListResize_();

      // Make sure the keyboard navigation tree is rebuilt whenever the
      // cr-lazy-list is updated.
      this.rebuildNavigationElements_();

      if (this.recordCountMetricsOnNextUpdate_ && this.hasLoadedData_ &&
          !noMetrics) {
        this.recordBookmarkCountMetrics_();
      }
    });
  }

  private onSearchChanged_() {
    this.recordCountMetricsOnNextUpdate_ = true;
  }

  private updateListScrollOffset_() {
    // Set scrollOffset so the cr-lazy-list scrolling accounts for the space the
    // other scrolling UI elements take.
    this.updateComplete.then(() => {
      const bookmarksOffsetTop = this.$.bookmarks.offsetTop;
      this.$.list.scrollOffset = this.$.list.offsetTop - bookmarksOffsetTop;
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
    recordViewType(this.compact_);
    recordSearchCTR(SearchAction.SHOWN);
    this.recordCountMetricsOnNextUpdate_ = true;
  }

  private rebuildNavigationElements_() {
    if (this.rebuildNavigationElementsTimerId_ !== -1) {
      clearTimeout(this.rebuildNavigationElementsTimerId_);
    }
    this.rebuildNavigationElementsTimerId_ = setTimeout(() => {
      this.rebuildNavigationElementsTimerId_ = -1;
      this.keyArrowNavigationService_.rebuildNavigationElements();
      this.fire('rebuild-navigation-elements');
      if (this.recordCountMetricsOnNextUpdate_ && this.hasLoadedData_) {
        this.recordBookmarkCountMetricsInternal_();
        this.recordCountMetricsOnNextUpdate_ = false;
      }
    }, 1);
  }

  private recordBookmarkCountMetrics_() {
    this.recordCountMetricsOnNextUpdate_ = true;
    this.rebuildNavigationElements_();
  }

  private recordBookmarkCountMetricsInternal_() {
    recordBookmarksShown(
        this.keyArrowNavigationService_.getElementCount(),
        this.hasSomeActiveFilter);
    this.fire('bookmark-count-recorded');
  }

  /**
   * Recursively removes all descendant folder IDs of a collapsed folder from
   * the expandedFolderIds_ Set, so they are initialized collapsed when
   * re-opened.
   */
  private collapseDescendants_(folder: BookmarksTreeNode) {
    if (!folder.children) {
      return;
    }
    for (const child of folder.children) {
      if (!child.url) {
        this.expandedFolderIds_.delete(child.id);
        this.collapseDescendants_(child);
      }
    }
  }

  protected onPowerBookmarkToggle_(event: CustomEvent<{
    bookmark: BookmarksTreeNode,
    expanded: boolean,
  }>) {
    const {bookmark, expanded} = event.detail;
    if (expanded) {
      this.expandedFolderIds_.add(bookmark.id);
    } else {
      this.expandedFolderIds_.delete(bookmark.id);
      this.collapseDescendants_(bookmark);
    }
    this.updateDisplayList_(/* noMetrics =*/ true);
    this.notifyBookmarksListResize_();
    this.recordBookmarkCountMetrics_();
  }

  protected onListItemSizeChanged_() {
    this.notifyBookmarksListResize_();
  }

  /**
   * Focuses the parent row of a child row, delegating focus backward traversal.
   */
  protected onPowerBookmarkRowFocusParent_(e: CustomEvent<{parentId: string}>) {
    const parentElement = this.shadowRoot.querySelector<HTMLElement>(
        `#bookmark-${e.detail.parentId}`);
    if (parentElement) {
      parentElement.focus();
      this.keyArrowNavigationService_.setCurrentFocusIndex(parentElement);
    }
  }

  /**
   * Invoked when the user clicks a power bookmarks row. This will either
   * display children in the case of a folder row, or open the URL in the case
   * of a bookmark row.
   */
  protected onRowClicked_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (!this.editing) {
      if (event.detail.bookmark.children) {
        this.recordCountMetricsOnNextUpdate_ = true;
        this.activeFolderPath =
            [...this.activeFolderPath, event.detail.bookmark];
        this.dispatchActiveFolderPathChanged_();
        this.fire('clear-search');
        this.updateComplete.then(async () => {
          if (this.displayList_.length > 0) {
            const element = await this.$.list.ensureItemRendered(0);
            element.focus();
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
    // TODO(crbug.com/493823435): Determine if this iron-list related workaround
    // is still necessary now that the list has been migrated to cr-lazy-list.
    if (event.target) {
      (event.target as HTMLElement).blur();
    }
  }

  protected onCheckboxChange_(
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
      this.fire('row-selected-change', {
        id: event.detail.bookmark.id,
        checked: event.detail.checked,
      });
    }
  }

  protected onInputChange_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, value: string|null}>) {
    const newName = event.detail.value;
    if (newName != null) {
      this.bookmarksApi_.renameBookmark(event.detail.bookmark.id, newName);
    }
    this.renamingId = '';
  }

  protected getRowHeading_(index: number): string {
    const showHeadings =
        this.sectionVisibility_ && this.sectionVisibility_.filterHeadings;
    if (!showHeadings) {
      return '';
    }

    if (index === this.firstSecondaryIndex_) {
      return loadTimeData.getString('secondaryFilterHeading');
    }

    if (index === 0) {
      return loadTimeData.getStringF(
          'primaryFilterHeading', this.getActiveFolderLabel_());
    }

    return '';
  }

  protected notifyBookmarksListResize_() {
    this.$.list.fillCurrentViewport();
  }

  private getSelectedDescription_() {
    return loadTimeData.getStringF(
        'selectedBookmarkCount', this.getSelectedBookmarksLength_());
  }

  protected getSelectedBookmarksList_(): BookmarksTreeNode[] {
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
  protected onBackClicked_() {
    this.recordCountMetricsOnNextUpdate_ = true;
    this.activeFolderPath = this.activeFolderPath.slice(0, -1);
    this.dispatchActiveFolderPathChanged_();
  }

  private dispatchActiveFolderPathChanged_() {
    this.fire('active-folder-path-changed', {value: this.activeFolderPath});
  }

  protected onShowContextMenu_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, event: MouseEvent}>) {
    this.fire('show-context-menu', event.detail);
  }

  protected onTrailingIconClicked_(
      event: CustomEvent<{bookmark: BookmarksTreeNode, event: MouseEvent}>) {
    this.onShowContextMenu_(event);
  }

  getParentFolder(): BookmarksTreeNode {
    return this.getActiveFolder() ||
        this.bookmarksService_.findBookmarkWithId(
            loadTimeData.getString('otherBookmarksId'))!;
  }

  protected onAddNewFolderClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const newParent = this.getParentFolder();
    if (editingDisabledByPolicy([newParent])) {
      this.fire('disabled-feature');
      return;
    }
    recordFolderAdded();
    this.bookmarksApi_
        .createFolder(newParent.id, loadTimeData.getString('newFolderTitle'))
        .then((result: {newFolderId: string}) => {
          this.renamingId = result.newFolderId;
        });
  }

  protected onViewToggled_() {
    this.compact_ = !this.compact_;
    if (this.compact_) {
      this.updateDisplayList_();
    }
    this.notifyBookmarksListResize_();
    recordViewType(this.compact_);
    const viewType = this.compact_ ? ViewType.kCompact : ViewType.kExpanded;
    this.bookmarksApi_.setViewType(viewType);
  }

  private computeHasShownBookmarks_(): boolean {
    return !!this.displayList_ && this.displayList_.length > 0;
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
    // The cr-lazy-list of `displayList_` is in a dynamically sized card.
    // Any time the size changes, let cr-lazy-list know so that cr-lazy-list can
    // properly adjust to its possibly new height.
    this.notifyBookmarksListResize_();

    this.fire(
        'has-scrollbars-changed',
        this.$.scroller.scrollHeight > this.$.scroller.offsetHeight);
  }

  protected onSortChanged_(
      e: CustomEvent<{index: number, sortOrder: SortOrder}>) {
    this.activeSortIndex = e.detail.index;
    this.sortOrder = e.detail.sortOrder;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list': PowerBookmarksListElement;
  }
}

customElements.define(PowerBookmarksListElement.is, PowerBookmarksListElement);
