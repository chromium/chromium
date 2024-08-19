// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains business logic for power bookmarks side panel content.

import type {BookmarkProductInfo} from '//resources/cr_components/commerce/shopping_service.mojom-webui.js';
import {PageImageServiceBrowserProxy} from '//resources/cr_components/page_image_service/browser_proxy.js';
import {ClientId as PageImageServiceClientId} from '//resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';

// This corresponds to the max number of concurrent ImageService requests
// before further requests get dropped. Further requests up to 600 should be
// batched by ImageService, but we leave this remainder as buffer in the case
// of multiple windows.
const MAX_IMAGE_SERVICE_REQUESTS = 30;

export interface Label {
  label: string;
  icon: string;
  active: boolean;
}

interface PowerBookmarksDelegate {
  setCurrentUrl(url: string|undefined): void;
  setImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode, url: string): void;
  onBookmarksLoaded(): void;
  onBookmarkChanged(id: string, changedInfo: chrome.bookmarks.ChangeInfo): void;
  onBookmarkCreated(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      parent: chrome.bookmarks.BookmarkTreeNode): void;
  onBookmarkMoved(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      oldParent: chrome.bookmarks.BookmarkTreeNode,
      newParent: chrome.bookmarks.BookmarkTreeNode): void;
  onBookmarkRemoved(bookmark: chrome.bookmarks.BookmarkTreeNode): void;
  getTrackedProductInfos(): {[key: string]: BookmarkProductInfo};
  getAvailableProductInfos(): Map<string, BookmarkProductInfo>;
  getSelectedBookmarks(): {[key: string]: boolean};
  getProductImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode): string;
}

export function editingDisabledByPolicy(
    bookmarks: chrome.bookmarks.BookmarkTreeNode[]) {
  if (!loadTimeData.getBoolean('editBookmarksEnabled')) {
    return true;
  }
  if (loadTimeData.getBoolean('hasManagedBookmarks')) {
    const managedNodeId = loadTimeData.getString('managedBookmarksFolderId');
    for (const bookmark of bookmarks) {
      if (bookmark.id === managedNodeId ||
          bookmark.parentId === managedNodeId) {
        return true;
      }
    }
  }
  return false;
}

// Return an array that includes folder and all its descendants.
export function getFolderDescendants(
    folder: chrome.bookmarks.BookmarkTreeNode,
    excludeFolder: chrome.bookmarks.BookmarkTreeNode|undefined =
        undefined): chrome.bookmarks.BookmarkTreeNode[] {
  if (folder === excludeFolder) {
    return [];
  }
  let expanded: chrome.bookmarks.BookmarkTreeNode[] = [folder];
  if (folder.children) {
    folder.children.forEach((child: chrome.bookmarks.BookmarkTreeNode) => {
      expanded = expanded.concat(getFolderDescendants(child, excludeFolder));
    });
  }
  return expanded;
}

// Compares bookmarks based on the newest dateAdded of the bookmark
// itself and all descendants.
function compareNewest(
    a: chrome.bookmarks.BookmarkTreeNode,
    b: chrome.bookmarks.BookmarkTreeNode): number {
  let aValue: number|undefined;
  let bValue: number|undefined;
  getFolderDescendants(a).forEach((descendant) => {
    if (!aValue || descendant.dateAdded! > aValue) {
      aValue = descendant.dateAdded;
    }
  });
  getFolderDescendants(b).forEach((descendant) => {
    if (!bValue || descendant.dateAdded! > bValue) {
      bValue = descendant.dateAdded;
    }
  });
  return bValue! - aValue!;
}

// Compares bookmarks based on the oldest dateAdded of the bookmark
// itself and all descendants.
function compareOldest(
    a: chrome.bookmarks.BookmarkTreeNode,
    b: chrome.bookmarks.BookmarkTreeNode): number {
  let aValue: number|undefined;
  let bValue: number|undefined;
  getFolderDescendants(a).forEach((descendant) => {
    if (!aValue || descendant.dateAdded! < aValue) {
      aValue = descendant.dateAdded;
    }
  });
  getFolderDescendants(b).forEach((descendant) => {
    if (!bValue || descendant.dateAdded! < bValue) {
      bValue = descendant.dateAdded;
    }
  });
  return aValue! - bValue!;
}

// Compares bookmarks based on the most recent dateLastUsed, or dateAdded if
// dateUsed is not set, of the bookmark itself and all descendants.
function compareLastOpened(
    a: chrome.bookmarks.BookmarkTreeNode,
    b: chrome.bookmarks.BookmarkTreeNode): number {
  let aValue: number|undefined;
  let bValue: number|undefined;
  getFolderDescendants(a).forEach((descendant) => {
    const descendantValue = descendant.dateLastUsed ? descendant.dateLastUsed :
                                                      descendant.dateAdded!;
    if (!aValue || descendantValue > aValue) {
      aValue = descendantValue;
    }
  });
  getFolderDescendants(b).forEach((descendant) => {
    const descendantValue = descendant.dateLastUsed ? descendant.dateLastUsed :
                                                      descendant.dateAdded!;
    if (!bValue || descendantValue > bValue) {
      bValue = descendantValue;
    }
  });
  return bValue! - aValue!;
}

function compareAlphabetical(
    a: chrome.bookmarks.BookmarkTreeNode,
    b: chrome.bookmarks.BookmarkTreeNode): number {
  return a.title!.localeCompare(b.title);
}

function compareReverseAlphabetical(
    a: chrome.bookmarks.BookmarkTreeNode,
    b: chrome.bookmarks.BookmarkTreeNode): number {
  return b.title!.localeCompare(a.title);
}

export class PowerBookmarksService {
  private delegate_: PowerBookmarksDelegate;
  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private listeners_ = new Map<string, Function>();
  private folders_: chrome.bookmarks.BookmarkTreeNode[] = [];
  private bookmarksWithCachedImages_ = new Set<string>();
  private activeImageServiceRequestCount_: number = 0;
  private inactiveImageServiceRequests_ =
      new Map<string, chrome.bookmarks.BookmarkTreeNode>();
  private maxImageServiceRequests_ = MAX_IMAGE_SERVICE_REQUESTS;

  constructor(delegate: PowerBookmarksDelegate) {
    this.delegate_ = delegate;
  }

  /**
   * Creates listeners for all relevant bookmark and shopping information.
   * Invoke during setup.
   */
  startListening() {
    this.bookmarksApi_.getActiveUrl().then(
        url => this.delegate_.setCurrentUrl(url));
    this.bookmarksApi_.getFolders().then(folders => {
      this.folders_ = folders;
      this.addListener_(
          'onChanged',
          (id: string, changedInfo: chrome.bookmarks.ChangeInfo) =>
              this.onChanged_(id, changedInfo));
      this.addListener_(
          'onCreated',
          (_id: string, node: chrome.bookmarks.BookmarkTreeNode) =>
              this.onCreated_(node));
      this.addListener_(
          'onMoved',
          (_id: string, movedInfo: chrome.bookmarks.MoveInfo) =>
              this.onMoved_(movedInfo));
      this.addListener_('onRemoved', (id: string) => this.onRemoved_(id));
      this.addListener_('onTabActivated', (_info: chrome.tabs.ActiveInfo) => {
        this.bookmarksApi_.getActiveUrl().then(
            url => this.delegate_.setCurrentUrl(url));
      });
      this.addListener_(
          'onTabUpdated',
          (_tabId: number, _changeInfo: object, tab: chrome.tabs.Tab) => {
            if (tab.active) {
              this.delegate_.setCurrentUrl(tab.url);
            }
          });
      this.delegate_.onBookmarksLoaded();
    });
  }

  /**
   * Cleans up any listeners created by the startListening method.
   * Invoke during teardown.
   */
  stopListening() {
    for (const [eventName, callback] of this.listeners_.entries()) {
      this.bookmarksApi_.callbackRouter[eventName]!.removeListener(callback);
    }
  }

  /**
   * Returns a list of all root bookmark folders.
   */
  getFolders() {
    return this.folders_;
  }

  /**
   * Returns a list of all bookmarks defaulted to if no filter criteria are
   * provided.
   */
  getTopLevelBookmarks() {
    return this.filterBookmarks(undefined, 0, undefined, []);
  }

  /**
   * Returns a list of bookmarks and folders filtered by the provided criteria.
   */
  filterBookmarks(
      activeFolder: chrome.bookmarks.BookmarkTreeNode|undefined,
      activeSortIndex: number, searchQuery: string|undefined, labels: Label[],
      excludeFolder: chrome.bookmarks.BookmarkTreeNode|
      undefined = undefined): chrome.bookmarks.BookmarkTreeNode[] {
    let bookmarks: chrome.bookmarks.BookmarkTreeNode[] = [];
    if (activeFolder) {
      bookmarks = activeFolder.children!.slice();
    } else {
      let topLevelBookmarks: chrome.bookmarks.BookmarkTreeNode[] = [];
      this.folders_.forEach(
          folder => topLevelBookmarks = topLevelBookmarks.concat(
              (folder.id === loadTimeData.getString('otherBookmarksId') ||
               folder.id === loadTimeData.getString('mobileBookmarksId')) ?
                  folder.children! :
                  [folder]));
      bookmarks = topLevelBookmarks;
    }
    if (searchQuery || labels.find((label) => label.active)) {
      bookmarks = this.applySearchQueryAndLabels_(
          labels, searchQuery, bookmarks, excludeFolder);
    }
    const sortChangedPosition = this.sortBookmarks(bookmarks, activeSortIndex);
    return sortChangedPosition ? bookmarks.slice() : bookmarks;
  }

  /**
   * Apply the current active sort type to the given bookmarks list. Returns
   * true if any elements in the list changed position.
   */
  sortBookmarks(
      bookmarks: chrome.bookmarks.BookmarkTreeNode[],
      activeSortIndex: number): boolean {
    let changedPosition = false;
    bookmarks.sort(function(
        a: chrome.bookmarks.BookmarkTreeNode,
        b: chrome.bookmarks.BookmarkTreeNode) {
      // Always sort by folders first
      if (!a.url && b.url) {
        return -1;
      } else if (a.url && !b.url) {
        changedPosition = true;
        return 1;
      } else {
        let toReturn;
        if (activeSortIndex === 0) {
          toReturn = compareNewest(a, b);
        } else if (activeSortIndex === 1) {
          toReturn = compareOldest(a, b);
        } else if (activeSortIndex === 2) {
          toReturn = compareLastOpened(a, b);
        } else if (activeSortIndex === 3) {
          toReturn = compareAlphabetical(a, b);
        } else {
          toReturn = compareReverseAlphabetical(a, b);
        }
        if (toReturn > 0) {
          changedPosition = true;
        }
        return toReturn;
      }
    });
    return changedPosition;
  }

  /**
   * Checks bookmarks for any relevant data and updates delegate_ with the
   * results. Used to batch data fetching in any cases where it is particularly
   * expensive.
   */
  async refreshDataForBookmarks(bookmarks:
                                    chrome.bookmarks.BookmarkTreeNode[]) {
    bookmarks.forEach(
        (bookmark) => this.findBookmarkImageUrls_(bookmark, true, false));
  }

  /**
   * Returns the BookmarkTreeNode with the given id, or undefined if one does
   * not exist.
   */
  findBookmarkWithId(id: string|undefined): chrome.bookmarks.BookmarkTreeNode
      |undefined {
    if (id) {
      const path = this.findPathToId(id);
      if (path) {
        return path[path.length - 1];
      }
    }
    return undefined;
  }

  /**
   * Returns true if the given url is not already present in the given folder.
   * If the folder is undefined, will default to the "Other Bookmarks" folder.
   */
  canAddUrl(
      url: string|undefined,
      folder: chrome.bookmarks.BookmarkTreeNode|undefined): boolean {
    if (!folder) {
      folder =
          this.findBookmarkWithId(loadTimeData.getString('otherBookmarksId'));
      if (!folder) {
        return false;
      }
    }
    return folder.children!.findIndex(b => b.url === url) === -1;
  }

  bookmarkMatchesSearchQueryAndLabels(
      bookmark: chrome.bookmarks.BookmarkTreeNode, labels: Label[],
      searchQuery: string|undefined): boolean {
    return this.nodeMatchesContentFilters_(bookmark, labels) &&
        (!searchQuery ||
         (!!bookmark.title &&
          bookmark.title.toLocaleLowerCase().includes(searchQuery!)) ||
         (!!bookmark.url &&
          bookmark.url.toLocaleLowerCase().includes(searchQuery!)));
  }

  setMaxImageServiceRequestsForTesting(max: number) {
    this.maxImageServiceRequests_ = max;
  }

  getPriceTrackedInfo(bookmark: chrome.bookmarks.BookmarkTreeNode):
      BookmarkProductInfo|undefined {
    const trackedProductInfos = this.delegate_.getTrackedProductInfos();
    const priceTrackValue = Object.entries(trackedProductInfos)
                                .find(([key, _val]) => key === bookmark.id)
                                ?.[1];
    return priceTrackValue;
  }

  getAvailableProductInfo(bookmark: chrome.bookmarks.BookmarkTreeNode):
      BookmarkProductInfo|undefined {
    const availableProductInfos = this.delegate_.getAvailableProductInfos();
    return availableProductInfos.get(bookmark.id);
  }

  bookmarkIsSelected(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    const selectedBookmarks = this.delegate_.getSelectedBookmarks();
    return Object.entries(selectedBookmarks)
               .find(([key, _val]) => key === bookmark.id)?.[1] ?? false;
  }


  private applySearchQueryAndLabels_(
      labels: Label[], searchQuery: string|undefined,
      shownBookmarks: chrome.bookmarks.BookmarkTreeNode[],
      excludeFolder: chrome.bookmarks.BookmarkTreeNode|
      undefined): chrome.bookmarks.BookmarkTreeNode[] {
    let searchSpace: chrome.bookmarks.BookmarkTreeNode[] = [];
    // Search space should include all descendants of the shown bookmarks, in
    // addition to the shown bookmarks themselves, excluding the excludeFolder
    // and its descendants.
    shownBookmarks.forEach((bookmark: chrome.bookmarks.BookmarkTreeNode) => {
      searchSpace =
          searchSpace.concat(getFolderDescendants(bookmark, excludeFolder));
    });
    return searchSpace.filter(
        (bookmark: chrome.bookmarks.BookmarkTreeNode) =>
            this.bookmarkMatchesSearchQueryAndLabels(
                bookmark, labels, searchQuery));
  }

  private nodeMatchesContentFilters_(
      bookmark: chrome.bookmarks.BookmarkTreeNode, labels: Label[]): boolean {
    // Price tracking label
    const isPriceTracked = !!this.getPriceTrackedInfo(bookmark);
    if (labels[0] && labels[0]!.active && !isPriceTracked) {
      return false;
    }
    return true;
  }

  private addListener_(eventName: string, callback: Function): void {
    this.bookmarksApi_.callbackRouter[eventName]!.addListener(callback);
    this.listeners_.set(eventName, callback);
  }

  private onChanged_(id: string, changedInfo: chrome.bookmarks.ChangeInfo) {
    const bookmark = this.findBookmarkWithId(id)!;
    Object.assign(bookmark, changedInfo);
    // Deep copy is necessary to ensure that the original bookmark object is
    // not directly mutated. This helps LitElement's change detection recognize
    // the changes since the reference to the object will change.
    const deepCopyBookmark = structuredClone(bookmark);
    const parent = this.findBookmarkWithId(bookmark.parentId);
    if (parent) {
      const index =
          parent.children!.findIndex(child => child.id === bookmark.id);
      parent.children![index] = deepCopyBookmark;
    }
    this.findBookmarkImageUrls_(deepCopyBookmark, false, true);
    this.delegate_.onBookmarkChanged(id, changedInfo);
  }

  private onCreated_(node: chrome.bookmarks.BookmarkTreeNode) {
    const parent = this.findBookmarkWithId(node.parentId as string)!;
    if (!node.url && !node.children) {
      // Newly created folders in this session may not have an array of
      // children yet, so create an empty one.
      node.children = [];
    }
    parent.children!.splice(node.index!, 0, node);
    this.delegate_.onBookmarkCreated(node, parent);
    this.findBookmarkImageUrls_(node, false, false);
  }

  private onMoved_(movedInfo: chrome.bookmarks.MoveInfo) {
    // Remove node from oldParent at oldIndex.
    const oldParent = this.findBookmarkWithId(movedInfo.oldParentId)!;
    const movedNode = oldParent!.children![movedInfo.oldIndex]!;
    Object.assign(
        movedNode, {index: movedInfo.index, parentId: movedInfo.parentId});
    oldParent.children!.splice(movedInfo.oldIndex, 1);

    // Add the node to the new parent at index.
    const newParent = this.findBookmarkWithId(movedInfo.parentId)!;
    if (!newParent.children) {
      newParent.children = [];
    }
    newParent.children!.splice(movedInfo.index, 0, movedNode);
    this.delegate_.onBookmarkMoved(movedNode, oldParent, newParent);
  }

  private onRemoved_(id: string) {
    const oldPath = this.findPathToId(id);
    const removedNode = oldPath.pop()!;
    const oldParent = oldPath[oldPath.length - 1]!;
    oldParent.children!.splice(oldParent.children!.indexOf(removedNode), 1);
    this.delegate_.onBookmarkRemoved(removedNode);
  }

  /**
   * Finds the node within all bookmarks and returns the path to the node in
   * the tree.
   */
  findPathToId(id: string): chrome.bookmarks.BookmarkTreeNode[] {
    const path: chrome.bookmarks.BookmarkTreeNode[] = [];

    function findPathByIdInternal(
        id: string, node: chrome.bookmarks.BookmarkTreeNode) {
      if (node.id === id) {
        path.push(node);
        return true;
      }

      if (!node.children) {
        return false;
      }

      path.push(node);
      const foundInChildren =
          node.children.some(child => findPathByIdInternal(id, child));
      if (!foundInChildren) {
        path.pop();
      }

      return foundInChildren;
    }

    this.folders_.some(bookmark => findPathByIdInternal(id, bookmark));
    return path;
  }

  /**
   * Assigns an image url for the given bookmark. Also assigns an image url to
   * all children if recurse is true.
   */
  private async findBookmarkImageUrls_(
      bookmark: chrome.bookmarks.BookmarkTreeNode, recurse: boolean,
      forceUpdate: boolean) {
    const hasImage =
        this.bookmarksWithCachedImages_.has(bookmark.id.toString());
    if (forceUpdate || !hasImage) {
      // Reset image url to ensure old images don't persist while the new image
      // is being fetched.
      this.delegate_.setImageUrl(bookmark, '');
      if (bookmark.url) {
        const productImageUrl = this.delegate_.getProductImageUrl(bookmark);
        if (productImageUrl) {
          this.delegate_.setImageUrl(bookmark, productImageUrl);
          this.bookmarksWithCachedImages_.add(bookmark.id.toString());
        } else {
          if (this.activeImageServiceRequestCount_ <
              this.maxImageServiceRequests_) {
            this.findBookmarkImageUrl_(bookmark);
          } else {
            this.inactiveImageServiceRequests_.set(bookmark.id, bookmark);
          }
        }
      }
    }
    if (recurse && bookmark.children) {
      bookmark.children.forEach(
          child => this.findBookmarkImageUrls_(child, false, forceUpdate));
    }
  }

  private async findBookmarkImageUrl_(bookmark:
                                          chrome.bookmarks.BookmarkTreeNode) {
    this.inactiveImageServiceRequests_.delete(bookmark.id);

    if (!bookmark.url || !loadTimeData.getBoolean('urlImagesEnabled')) {
      return;
    }

    const url: Url = {url: bookmark.url};

    // Fetch the representative image for this page, if possible.
    this.activeImageServiceRequestCount_++;
    // TODO(b/303613231): Update this code to distinguish account bookmarks
    // (which can get images from PageImageService) from local bookmarks (which
    // can't), once the account bookmark store exists. The "is account bookmark"
    // bit will likely need to be plumbed here. (For reference:
    // crrev.com/c/5346717 made the equivalent change for Android.)
    const {result} =
        await PageImageServiceBrowserProxy.getInstance()
            .handler.getPageImageUrl(
                PageImageServiceClientId.Bookmarks, url,
                {suggestImages: false, optimizationGuideImages: true});
    this.activeImageServiceRequestCount_--;

    if (result) {
      this.delegate_.setImageUrl(bookmark, result.imageUrl.url);
      this.bookmarksWithCachedImages_.add(bookmark.id.toString());
    }

    if (this.inactiveImageServiceRequests_.size > 0) {
      this.findBookmarkImageUrl_(
          this.inactiveImageServiceRequests_.values().next().value);
    }
  }
}
