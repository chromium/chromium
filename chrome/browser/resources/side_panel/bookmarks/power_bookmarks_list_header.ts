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
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {BookmarksTreeNode} from './bookmarks.mojom-webui.js';
import {SortOrder} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import type {BookmarksApiProxy} from './bookmarks_api_proxy.js';
import {getCss} from './power_bookmarks_list_header.css.js';
import {getHtml} from './power_bookmarks_list_header.html.js';
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

export class PowerBookmarksListHeaderElement extends CrLitElement {
  static get is() {
    return 'power-bookmarks-list-header';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      activeFolder: {type: Object},
      activeSortType_: {type: Object},
      activeSortIndex_: {type: Number},
      compact: {type: Boolean},
      disableEdit: {type: Boolean},
      editing: {type: Boolean},
      sortTypes_: {type: Array},
    };
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  accessor activeFolder: BookmarksTreeNode|undefined = undefined;
  accessor compact: boolean = false;
  accessor disableEdit: boolean = false;
  accessor editing: boolean = false;
  protected accessor activeSortType_: SortOption;
  protected accessor activeSortIndex_: number =
      loadTimeData.getInteger('sortOrder');
  protected accessor sortTypes_: SortOption[] = [
    {
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
    },
  ];
  private bookmarksService_: PowerBookmarksService =
      PowerBookmarksService.getInstance();

  constructor() {
    super();
    this.activeSortType_ = this.sortTypes_[this.activeSortIndex_]!;
  }

  override connectedCallback() {
    super.connectedCallback();
    recordSortType(this.sortTypes_[this.activeSortIndex_].sortOrder);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('activeSortIndex_')) {
      this.onActiveSortIndexChanged_();
    }
  }

  protected getBackButtonLabel_(): string {
    const parentFolder = this.bookmarksService_.findBookmarkWithId(
        this.activeFolder ? this.activeFolder.parentId : undefined);
    return loadTimeData.getStringF(
        'backButtonLabel', getFolderLabel(parentFolder));
  }

  protected shouldHideBackButton_(): boolean {
    return !this.activeFolder;
  }

  protected disableBackButton_(): boolean {
    return !this.activeFolder || this.editing;
  }

  protected getActiveFolderLabel_(): string {
    return getFolderLabel(this.activeFolder);
  }

  protected getViewButtonIcon_() {
    return this.compact ? 'bookmarks:compact-view' : 'bookmarks:visual-view';
  }

  protected getViewButtonTooltip_() {
    return this.compact ? loadTimeData.getString('compactView') :
                          loadTimeData.getString('visualView');
  }

  protected onBackButtonClick_() {
    this.fire('back-clicked');
  }

  protected onShowSortMenuClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.showAt(event.target as HTMLElement);
  }

  protected onViewToggleClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.fire('view-toggled');
  }

  protected onBulkEditClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.fire('bulk-edit');
  }

  private getSortMenuItemLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.label);
  }

  protected getSortMenuItemLowerLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.lowerLabel);
  }

  protected sortMenuItemIsSelected_(sortType: SortOption): boolean {
    return this.sortTypes_[this.activeSortIndex_].sortOrder ===
        sortType.sortOrder;
  }

  protected onSortTypeClick_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    const target = event.currentTarget as HTMLElement;
    const index = parseInt(target.dataset['index']!, 10);
    const sortOption = this.sortTypes_[index];
    this.activeSortIndex_ = index;
    this.bookmarksApi_.setSortOrder(sortOption.sortOrder);
    recordSortType(sortOption.sortOrder);
  }

  private onActiveSortIndexChanged_() {
    this.activeSortType_ = this.sortTypes_[this.activeSortIndex_];
    this.fire('sort-changed', {
      index: this.activeSortIndex_,
      sortOrder: this.sortTypes_[this.activeSortIndex_].sortOrder,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list-header': PowerBookmarksListHeaderElement;
  }
}

customElements.define(
    PowerBookmarksListHeaderElement.is, PowerBookmarksListHeaderElement);
