// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/card/card.js';
import 'chrome://resources/mwc/@material/web/divider/divider.js';
import 'chrome://resources/mwc/@material/web/icon/icon.js';
import 'chrome://resources/mwc/@material/web/iconbutton/icon-button.js';
import 'chrome://resources/mwc/@material/web/list/list.js';
import 'chrome://resources/mwc/@material/web/list/list-item.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './cra/cra-image.js';
import './cra/cra-menu.js';
import './cra/cra-menu-item.js';
import './recording-file-list-item.js';
import './recording-search-box.js';

import {
  classMap,
  createRef,
  css,
  html,
  PropertyDeclarations,
  ref,
  repeat,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {
  RecordingMetadata,
  RecordingMetadataMap,
} from '../core/recording_data_manager.js';
import {RecordingSortType, settings} from '../core/state/settings.js';
import {
  assertExhaustive,
  assertExists,
  assertInstanceof,
} from '../core/utils/assert.js';
import {
  getMonthLabel,
  getToday,
  getYesterday,
  isInThisMonth,
} from '../core/utils/datetime.js';
import {isObjectEmpty} from '../core/utils/utils.js';

import {CraMenu} from './cra/cra-menu.js';
import {RecordingFileListItem} from './recording-file-list-item.js';

interface RecordingSearchResult {
  highlight: [number, number]|null;
  recording: RecordingMetadata;
}

interface RecordingGroup {
  recordings: RecordingSearchResult[];
  sectionLabel: string;
}

// prettier-ignore
type RenderRecordingItem = {
  id: string,
}&({
  kind: 'header',
  label: string,
}|{
  kind: 'recording',
  searchHighlight: [number, number] | null,
  recording: RecordingMetadata,
});

interface InlinePlayingItemInfo {
  /**
   * The id of the current playing recording.
   */
  id: string;

  /**
   * Progress of the playback in range [0, 100].
   */
  progress: number;

  /**
   * Whether the playback is ongoing.
   */
  playing: boolean;
}

/**
 * A list of recording files.
 */
export class RecordingFileList extends ReactiveLitElement {
  static override styles = css`
    :host {
      background-color: var(--cros-sys-app_base_shaded);
      border-radius: 16px;
      display: flex;
      flex-flow: column;
      overflow: hidden;
      z-index: 0;
    }

    #header {
      align-items: center;
      box-sizing: border-box;
      display: flex;
      flex-flow: row;
      height: 80px;
      padding: 20px 16px 8px 32px;

      & > h1 {
        flex: 1;
        font: var(--cros-display-7-font);
        margin: 0;
      }
    }

    #list {
      display: flex;
      flex: 1;
      flex-flow: column;
      gap: 16px;
      overflow-y: auto;
      padding: 8px 0 calc(24px + var(--scroll-bottom-extra-padding, 0px));
    }

    #sort-recording-menu {
      --cros-menu-width: 200px;
    }

    .section-heading {
      color: var(--cros-sys-on_surface);
      font: var(--cros-title-1-font);
      margin: 16px 32px 0;
    }

    .illustration-container {
      align-items: center;
      display: flex;
      flex-flow: column;
      font: var(--cros-headline-1-font);
      gap: 16px;

      /* The height is full height minus footer size. */
      height: calc(100% - 48px);
      justify-content: center;

      @container style(--small-viewport: 1) {
        height: calc(100% - 32px);
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    recordingMetadataMap: {attribute: false},
    inlinePlayingItem: {attribute: false},
  };

  recordingMetadataMap: RecordingMetadataMap = {};

  inlinePlayingItem: InlinePlayingItemInfo|null = null;

  private readonly searchQuery = signal('');

  private readonly sortMenuRef = createRef<CraMenu>();

  private readonly sortMenuOpened = signal(false);

  private readonly platformHandler = usePlatformHandler();

  get firstRecordingForTest(): RecordingFileListItem {
    return assertExists(
      this.shadowRoot?.querySelector('recording-file-list-item'),
    );
  }

  recordingFileCountForTest(): number {
    return Object.keys(this.recordingMetadataMap).length;
  }

  /**
   * Try to focus onto the "more options" button of the recording.
   *
   * Does nothing if the specific recording doesn't exist.
   */
  focusOnOptionOfRecordingId(recordingId: string|null): void {
    if (recordingId === null) {
      return;
    }
    const item =
      this.shadowRoot?.querySelector(
        `recording-file-list-item[data-recording-id="${recordingId}"]`,
      ) ??
      null;
    if (item !== null) {
      assertInstanceof(item, RecordingFileListItem).focusOnOption();
    }
  }

  private onSortingTypeClick(newSortType: RecordingSortType) {
    settings.mutate((d) => {
      d.recordingSortType = newSortType;
    });
  }

  private renderSortMenu() {
    const onMenuOpen = () => {
      this.sortMenuOpened.value = true;
    };

    const onMenuClose = () => {
      this.sortMenuOpened.value = false;
    };

    return html`<cra-menu
      id="sort-recording-menu"
      anchor="sort-recording-button"
      ${ref(this.sortMenuRef)}
      @opened=${onMenuOpen}
      @closed=${onMenuClose}
    >
      <cra-menu-item
        headline=${i18n.recordingListSortByDateOption}
        ?checked=${settings.value.recordingSortType === RecordingSortType.DATE}
        data-role="menuitemradio"
        @cros-menu-item-triggered=${() => {
      this.onSortingTypeClick(RecordingSortType.DATE);
    }}
      >
      </cra-menu-item>
      <cra-menu-item
        headline=${i18n.recordingListSortByNameOption}
        ?checked=${settings.value.recordingSortType === RecordingSortType.NAME}
        data-role="menuitemradio"
        @cros-menu-item-triggered=${() => {
      this.onSortingTypeClick(RecordingSortType.NAME);
    }}
      >
      </cra-menu-item>
    </cra-menu>`;
  }

  private toggleSortMenu() {
    this.sortMenuRef.value?.toggle();
  }

  private renderHeader() {
    const onQueryChange = (ev: CustomEvent<string>) => {
      this.searchQuery.value = ev.detail;
    };

    const classes = {
      selected: this.sortMenuOpened.value,
    };
    return html`<div id="header">
        <h1>${i18n.recordingListHeader}</h1>
        <recording-search-box
          aria-label=${i18n.mainSearchLandmarkAriaLabel}
          role="search"
          @query-changed=${onQueryChange}
        >
        </recording-search-box>
        <cra-icon-button
          id="sort-recording-button"
          buttonstyle="filled"
          class="with-toggle-style ${classMap(classes)}"
          @click=${this.toggleSortMenu}
          aria-label=${i18n.recordingListSortButtonTooltip}
        >
          <cra-icon slot="icon" name="sort_by"></cra-icon>
        </cra-icon-button>
      </div>
      ${this.renderSortMenu()}`;
  }

  private groupRecordings(entries: RecordingSearchResult[]) {
    entries = entries.toSorted(
      (a, b) => b.recording.recordedAt - a.recording.recordedAt,
    );
    const today = getToday();
    const yesterday = getYesterday();
    const todayLabel = i18n.recordingListTodayHeader;
    const yesterdayLabel = i18n.recordingListYesterdayHeader;
    const thisMonthLabel = i18n.recordingListThisMonthHeader;

    const groups: RecordingGroup[] = [];
    let currentGroup: RecordingGroup = {
      sectionLabel: todayLabel,
      recordings: [],
    };

    for (const entry of entries) {
      const recordedAt = entry.recording.recordedAt;
      let dateGroup = getMonthLabel(
        this.platformHandler.getLocale(),
        recordedAt,
      );
      if (recordedAt >= today) {
        dateGroup = todayLabel;
      } else if (recordedAt >= yesterday) {
        dateGroup = yesterdayLabel;
      } else if (isInThisMonth(recordedAt)) {
        dateGroup = thisMonthLabel;
      }

      if (dateGroup !== currentGroup.sectionLabel) {
        // Skip pushing if there are no recordings in the current group.
        if (currentGroup.recordings.length > 0) {
          groups.push(currentGroup);
        }
        currentGroup = {
          sectionLabel: dateGroup,
          recordings: [entry],
        };
      } else {
        currentGroup.recordings.push(entry);
      }
    }
    if (currentGroup.recordings.length > 0) {
      groups.push(currentGroup);
    }
    return groups;
  }

  private searchRecordings(
    recordings: RecordingMetadata[],
  ): RecordingSearchResult[] {
    const query = this.searchQuery.value.trim().toLocaleLowerCase();

    // No active search, returns all recordings.
    if (query.length === 0) {
      return recordings.map((recording) => ({highlight: null, recording}));
    }

    const filteredEntries: RecordingSearchResult[] = [];
    for (const recording of recordings) {
      const lowerTitle = recording.title.toLocaleLowerCase();
      const startIndex = lowerTitle.indexOf(query);
      if (startIndex >= 0) {
        filteredEntries.push({
          highlight: [startIndex, startIndex + query.length],
          recording,
        });
      }
    }

    return filteredEntries;
  }

  private getRenderRecordingItems(): RenderRecordingItem[] {
    function recordingToRenderRecordingItem({
      highlight,
      recording,
    }: RecordingSearchResult): RenderRecordingItem {
      return {
        id: `record-${recording.id}`,
        kind: 'recording',
        searchHighlight: highlight,
        recording,
      };
    }

    const allRecordings = Object.values(this.recordingMetadataMap);
    const recordings = this.searchRecordings(allRecordings);

    // Sort most recent recordings first.
    if (settings.value.recordingSortType === RecordingSortType.DATE) {
      const recordingGroups = this.groupRecordings(recordings);

      return recordingGroups.flatMap((group) => {
        return [
          {
            id: `label-${group.sectionLabel}`,
            kind: 'header',
            label: group.sectionLabel,
          },
          ...group.recordings.map(
            (recording) => recordingToRenderRecordingItem(recording),
          ),
        ];
      });
    }

    // Sort by recording titles ascendingly (A to Z).
    if (settings.value.recordingSortType === RecordingSortType.NAME) {
      const sortedRecordings = recordings.sort(
        (a, b) => a.recording.title.localeCompare(b.recording.title),
      );
      return sortedRecordings.map(
        (recording) => recordingToRenderRecordingItem(recording),
      );
    }

    assertExhaustive(settings.value.recordingSortType);
  }

  private renderRecordingList() {
    const renderedItems = this.getRenderRecordingItems();
    if (renderedItems.length === 0) {
      return html`<div class="illustration-container">
        <cra-image name="recording_list_no_result_found"></cra-image>
        <div>${i18n.recordingListNoMatchText}</div>
      </div>`;
    }
    return repeat(
      renderedItems,
      (item) => item.id,
      (item) => {
        switch (item.kind) {
          case 'header':
            return html`<h2 class="section-heading">${item.label}</h2>`;
          case 'recording': {
            const {recording, searchHighlight} = item;
            const [playing, progress] = (() => {
              if (this.inlinePlayingItem === null ||
                  this.inlinePlayingItem.id !== recording.id) {
                return [false, null];
              }
              return [
                this.inlinePlayingItem.playing,
                this.inlinePlayingItem.progress,
              ];
            })();
            return html`
              <recording-file-list-item
                data-recording-id=${recording.id}
                .recording=${recording}
                .searchHighlight=${searchHighlight}
                .playing=${playing}
                .playProgress=${progress}
              >
              </recording-file-list-item>
            `;
          }
          default:
            assertExhaustive(item);
        }
      },
    );
  }

  override render(): RenderResult {
    if (isObjectEmpty(this.recordingMetadataMap)) {
      return html`
        <div class="illustration-container">
          <cra-image
            name="recording_list_empty"
            sizing="fit-container"
          ></cra-image>
        </div>
      `;
    }
    return [
      this.renderHeader(),
      html`<div
        id="list"
        aria-label=${i18n.mainRecordingsListLandmarkAriaLabel}
        role="main"
      >
        ${this.renderRecordingList()}
      </div>`,
    ];
  }
}

window.customElements.define('recording-file-list', RecordingFileList);

declare global {
  interface HTMLElementTagNameMap {
    'recording-file-list': RecordingFileList;
  }
}
