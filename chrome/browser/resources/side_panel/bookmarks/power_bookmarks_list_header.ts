// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_heading.js';

import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import {SortOrder} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {getTemplate} from './power_bookmarks_list_header.html.js';
import {recordSortType} from './power_bookmarks_metrics.js';
import {PowerBookmarksService} from './power_bookmarks_service.js';
import {getFolderLabel} from './power_bookmarks_utils.js';

interface SortOption {
  sortOrder: SortOrder;
  label: string;
  lowerLabel: string;
}

export interface PowerBookmarksListHeaderElement {
  $: {
    sortMenu: CrActionMenuElement,
  };
}

export class PowerBookmarksListHeaderElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-list-header';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeFolder: {
        type: Object,
        value: null,
      },

      activeSortType_: Object,

      activeSortIndex_: {
        type: Number,
        value: () => loadTimeData.getInteger('sortOrder'),
        observer: 'onActiveSortIndexChanged_',
      },

      compact: {
        type: Boolean,
        value: false,
      },

      disableEdit: {
        type: Boolean,
        value: false,
      },

      editing: {
        type: Boolean,
        value: false,
      },

      sortTypes_: {
        type: Array,
        value: () =>
            [{
              sortOrder: SortOrder.kNewest,
              label: loadTimeData.getString('sortNewest'),
              lowerLabel: loadTimeData.getString('sortNewestLower'),
            },
             {
               sortOrder: SortOrder.kOldest,
               label: loadTimeData.getString('sortOldest'),
               lowerLabel: loadTimeData.getString('sortOldestLower'),
             },
             {
               sortOrder: SortOrder.kLastOpened,
               label: loadTimeData.getString('sortLastOpened'),
               lowerLabel: loadTimeData.getString('sortLastOpenedLower'),
             },
             {
               sortOrder: SortOrder.kAlphabetical,
               label: loadTimeData.getString('sortAlphabetically'),
               lowerLabel: loadTimeData.getString('sortAlphabetically'),
             },
             {
               sortOrder: SortOrder.kReverseAlphabetical,
               label: loadTimeData.getString('sortReverseAlphabetically'),
               lowerLabel: loadTimeData.getString('sortReverseAlphabetically'),
             }],
      },
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  declare activeFolder: BookmarksTreeNode|undefined;
  declare compact: boolean;
  declare disableEdit: boolean;
  declare editing: boolean;
  declare private activeSortType_: SortOption;
  declare private activeSortIndex_: number;
  declare private sortTypes_: SortOption[];
  private bookmarksService_: PowerBookmarksService =
      PowerBookmarksService.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    recordSortType(this.sortTypes_[this.activeSortIndex_].sortOrder);
  }

  private getBackButtonLabel_(): string {
    const parentFolder = this.bookmarksService_.findBookmarkWithId(
        this.activeFolder ? this.activeFolder.parentId : undefined);
    return loadTimeData.getStringF(
        'backButtonLabel', getFolderLabel(parentFolder));
  }

  private shouldHideBackButton_(): boolean {
    return !this.activeFolder;
  }

  private disableBackButton_(): boolean {
    return !this.activeFolder || this.editing;
  }

  private getActiveFolderLabel_(): string {
    return getFolderLabel(this.activeFolder);
  }

  private getViewButtonIcon_() {
    return this.compact ? 'bookmarks:compact-view' : 'bookmarks:visual-view';
  }

  private getViewButtonTooltip_() {
    return this.compact ? loadTimeData.getString('compactView') :
                          loadTimeData.getString('visualView');
  }

  private onBackClicked_() {
    this.dispatchEvent(new CustomEvent('back-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  private onShowSortMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.showAt(event.target as HTMLElement);
  }

  private onViewToggleClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('view-toggled', {
      bubbles: true,
      composed: true,
    }));
  }

  private onBulkEditClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('bulk-edit-click', {
      bubbles: true,
      composed: true,
    }));
  }

  private getSortMenuItemLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.label);
  }

  private getSortMenuItemLowerLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.lowerLabel);
  }

  private sortMenuItemIsSelected_(sortType: SortOption): boolean {
    return this.sortTypes_[this.activeSortIndex_].sortOrder ===
        sortType.sortOrder;
  }

  private onSortTypeClicked_(event: DomRepeatEvent<SortOption>) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.activeSortIndex_ = event.model.index;
    this.bookmarksApi_.setSortOrder(event.model.item.sortOrder);
    recordSortType(event.model.item.sortOrder);
  }

  private onActiveSortIndexChanged_() {
    this.activeSortType_ = this.sortTypes_[this.activeSortIndex_];
    this.dispatchEvent(new CustomEvent('sort-changed', {
      bubbles: true,
      composed: true,
      detail: {
        index: this.activeSortIndex_,
        sortOrder: this.sortTypes_[this.activeSortIndex_].sortOrder,
      },
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list-header': PowerBookmarksListHeaderElement;
  }
}

customElements.define(
    PowerBookmarksListHeaderElement.is, PowerBookmarksListHeaderElement);
