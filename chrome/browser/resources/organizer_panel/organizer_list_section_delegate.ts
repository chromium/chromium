// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';

import type {OrganizerListSectionItem} from './organizer_list_section_item.js';

// LINT.IfChange(OrganizerPanelCloseAction)
/**
 * These values are persisted to logs and should not be renumbered or reused.
 * See tools/metrics/histograms/metadata/tab/enums.xml.
 */
export enum OrganizerPanelCloseAction {
  NO_ACTION = 0,
  CLOSED_TAB = 1,
  OPEN_RECENTLY_CLOSED = 2,
  SWITCH_TO_TAB = 3,
  OPEN_CROSS_DEVICE_TAB = 4,
  CREATE_TAB_GROUP = 5,
  OPEN_TAB_GROUP = 6,
  OPEN_TAB_GROUP_MENU = 7,
  OPEN_CROSS_DEVICE_PROMO = 8,
  COUNT = OPEN_CROSS_DEVICE_PROMO + 1,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:OrganizerPanelCloseAction)

let pendingCloseAction: OrganizerPanelCloseAction =
    OrganizerPanelCloseAction.NO_ACTION;
let numTabsClosed: number = 0;

export function setCloseAction(action: OrganizerPanelCloseAction) {
  pendingCloseAction = action;
}

export function incrementNumTabsClosed(count: number = 1) {
  numTabsClosed += count;
}

export function resetCloseMetrics() {
  pendingCloseAction = OrganizerPanelCloseAction.NO_ACTION;
  numTabsClosed = 0;
}

export function recordCloseMetrics() {
  chrome.histograms.recordEnumerationValue(
      'Tabs.Organizer.CloseAction', pendingCloseAction,
      OrganizerPanelCloseAction.COUNT);
  chrome.histograms.recordSmallCount(
      'Tabs.Organizer.NumTabsClosedPerInstance', numTabsClosed);
  resetCloseMetrics();
}

// Interface implemented by the section element to receive update events.
export interface OrganizerListSectionClient {
  onItemsChanged(items: Array<OrganizerListSectionItem<unknown>>): void;
}

// LINT.IfChange(OrganizerAction)
export type OrganizerAction = 'CloseTab'|'OpenCrossDeviceTab'|
    'OpenRecentlyClosedEntry'|'OpenTabGroup'|'SwitchTab';
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/histograms.xml:OrganizerAction)

// LINT.IfChange(OrganizerSection)
export type OrganizerSection =
    'CrossDeviceTabs'|'OpenTabs'|'RecentlyClosed'|'TabGroups';
// LINT.ThenChange(//tools/metrics/histograms/metadata/tab/histograms.xml:OrganizerSection)

// Delegate for a section in the organizer list.
export interface OrganizerListSectionDelegate<T> {
  // Hooks up the section element to receive updates from this section's model.
  init(sectionClient: OrganizerListSectionClient): void;

  // Returns the unique identifier for this section.
  getId(): string;

  // Returns the section header.
  getHeader(): string;

  // Returns the section name used in UMA histogram names.
  getHistogramSectionName(): OrganizerSection;

  // Returns all items that should be shown in the section.
  getItems(): Promise<Array<OrganizerListSectionItem<T>>>;

  // Called when an item in this section is clicked.
  onItemClick(item: OrganizerListSectionItem<T>): void;

  // Returns the action name used in UMA histograms when an item is clicked.
  getItemClickAction(): OrganizerAction;

  // Optional: Called when an item's trailing action button is clicked.
  onItemActionButtonClicked?
      (item: OrganizerListSectionItem<T>, buttonElement: HTMLElement):
          Promise<void>|void;

  // Optional: Returns the action name used in UMA histograms when an item's
  // trailing action button is clicked.
  getActionButtonClickAction?(): OrganizerAction;

  // Optional: Called when an item's context menu is triggered via right-click.
  onItemContextMenuClicked?
      (item: OrganizerListSectionItem<T>, x: number, y: number): Promise<void>|
      void;

  // Optional: Called to get the component rendered when no items are rendered
  // in this section, or at the top of the section if
  // `shouldAlwaysShowZeroState()` returns true.
  getZeroState?(): TemplateResult;

  // Optional: Returns true if the zero state component should always be
  // rendered at the top of this section when not searching, even when items
  // exist.
  shouldAlwaysShowZeroState?(): boolean;

  // Optional: Returns true if the section should be hidden.
  shouldHide?(): boolean;
}
