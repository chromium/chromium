// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/card/card.js';
import 'chrome://resources/cros_components/menu/menu_item.js';
import 'chrome://resources/mwc/@material/web/divider/divider.js';
import 'chrome://resources/mwc/@material/web/icon/icon.js';
import 'chrome://resources/mwc/@material/web/iconbutton/icon-button.js';
import 'chrome://resources/mwc/@material/web/list/list.js';
import 'chrome://resources/mwc/@material/web/list/list-item.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './cra/cra-menu.js';

import {Menu} from 'chrome://resources/cros_components/menu/menu.js';
import {
  createRef,
  css,
  html,
  map,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {
  ReactiveLitElement,
} from '../core/reactive/lit.js';
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
  formatDate,
  formatDuration,
  formatTime,
  getMonthLabel,
  getToday,
  getYesterday,
  isInThisMonth,
} from '../core/utils/datetime.js';
import {stopPropagation} from '../core/utils/event_handler.js';

interface RecordingGroup {
  recordings: RecordingMetadata[];
  sectionLabel: string;
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

    .recording {
      --cros-card-padding: 24px;
      --cros-card-hover-color: none;

      position: relative;

      & > cros-card {
        background-color: var(--cros-sys-app_base);
        min-height: initial;
        width: initial;
        -webkit-tap-highlight-color: transparent;

        &::part(content) {
          align-items: start;
          cursor: pointer;
          display: flex;
          flex-flow: row;
          gap: 16px;
        }
      }

      & > .options {
        inset-inline-end: 12px;
        position: absolute;
        top: 12px;
        z-index: 1;
      }
    }

    .recording-info {
      align-items: stretch;
      display: flex;
      flex: 1;
      flex-flow: column;
      gap: 8px;
      min-width: 0;
    }

    .title {
      font: var(--cros-title-1-font);
      overflow: hidden;

      /* To avoid overlap with the options button. */
      padding-inline-end: 36px;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .timeline {
      background-color: var(--cros-sys-primary);
      border-radius: 2px;
      height: 4px;
      margin-top: 16px;
    }

    .timestamps {
      display: flex;
      flex-flow: row;
      font: var(--cros-body-2-font);
      gap: 24px;

      & > span:first-child {
        flex: 1;
      }
    }

    #header {
      align-items: center;
      box-sizing: border-box;
      display: flex;
      flex-flow: row;
      height: 80px;
      padding: 20px 16px 8px 32px;

      & > span {
        flex: 1;
        font: var(--cros-display-7-font);
      }
    }

    #list {
      display: flex;
      flex: 1;
      flex-flow: column;
      gap: 16px;
      overflow-y: auto;
      padding: 8px 32px 24px;
    }

    #sort-recording-menu {
      --cros-menu-width: 200px;
    }

    .section-heading {
      color: var(--cros-sys-on_surface);
      font: var(--cros-title-1-font);
      padding-top: 16px;
    }
  `;

  static override properties: PropertyDeclarations = {
    recordingMetadataMap: {attribute: false},
  };

  recordingMetadataMap: RecordingMetadataMap = {};

  private readonly sortMenuRef = createRef<Menu>();

  private onRecordingClick(ev: PointerEvent) {
    const target = assertInstanceof(ev.currentTarget, HTMLElement);
    const id = assertExists(target.dataset['recordingId']);
    this.dispatchEvent(new CustomEvent('recording-clicked', {detail: id}));
  }

  // TODO: b/336963138 - Add back action menu for delete.
  // private onDeleteRecordingClick(id: string) {
  //   this.dispatchEvent(
  //       new CustomEvent('delete-recording-clicked', {detail: id}),
  //   );
  // }

  private onSortingTypeClicked(newSortType: RecordingSortType) {
    settings.mutate((d) => {
      d.recordingSortType = newSortType;
    });
  }

  private renderSortMenu() {
    return html`<cra-menu
      id="sort-recording-menu"
      anchor="sort-recording-button"
      ${ref(this.sortMenuRef)}
    >
      <cros-menu-item
        headline=${i18n.recordingListSortByDateOption}
        ?checked=${settings.value.recordingSortType === RecordingSortType.DATE}
        @cros-menu-item-triggered=${() => {
      this.onSortingTypeClicked(RecordingSortType.DATE);
    }}
      >
      </cros-menu-item>
      <cros-menu-item
        headline=${i18n.recordingListSortByNameOption}
        ?checked=${settings.value.recordingSortType === RecordingSortType.NAME}
        @cros-menu-item-triggered=${() => {
      this.onSortingTypeClicked(RecordingSortType.NAME);
    }}
      >
      </cros-menu-item>
    </cra-menu>`;
  }

  private renderHeader() {
    return html`<div id="header">
      <span>${i18n.recordingListHeader}</span>
      <cra-icon-button buttonstyle="floating">
        <!-- TODO: b/336963138 - Implements search -->
        <cra-icon slot="icon" name="search"></cra-icon>
      </cra-icon-button>
      <cra-icon-button
        id="sort-recording-button"
        buttonstyle="floating"
        @click=${() => {
      this.sortMenuRef.value?.show();
    }}
      >
        <cra-icon slot="icon" name="sort_by"></cra-icon>
        <!-- TODO: b/336963138 - Add button tooltip -->
      </cra-icon-button>
    </div>
    ${this.renderSortMenu()}`;
  }

  private renderRecordingTimeline(recording: RecordingMetadata) {
    const recordingDurationDisplay = formatDuration({
      milliseconds: recording.durationMs,
    });
    // TODO: b/336963138 - Actually render which parts have speech.
    return [
      html`<div class="timeline"></div>`,
      html`<div class="timestamps">
        <span>
          ${formatDate(recording.recordedAt)} •
          ${formatTime(recording.recordedAt)}
        </span>
        <span>${recordingDurationDisplay}</span>
      </div>`,
    ];
  }

  private renderRecording(recording: RecordingMetadata) {
    function onPlayClick(ev: PointerEvent) {
      // TODO: b/336963138 - Implements inline playing.
      ev.preventDefault();
      ev.stopPropagation();
    }
    function onOptionsClick(ev: PointerEvent) {
      // TODO: b/336963138 - Implements options.
      ev.preventDefault();
      ev.stopPropagation();
    }
    // TODO(pihsun): Check why the ripple sometimes doesn't happen on touch
    // long-press but sometimes does.
    return html`<div class="recording">
      <cros-card
        @click=${this.onRecordingClick}
        data-recording-id=${recording.id}
        cardstyle="filled"
        tabindex="0"
        interactive
      >
        <cra-icon-button
          shape="circle"
          @click=${onPlayClick}
          @pointerdown=${stopPropagation}
        >
          <cra-icon slot="icon" name="play_arrow"></cra-icon>
        </cra-icon-button>
        <div class="recording-info">
          <div class="title">${recording.title}</div>
          ${this.renderRecordingTimeline(recording)}
        </div>
      </cros-card>
      <cra-icon-button
        buttonstyle="floating"
        class="options"
        @click=${onOptionsClick}
      >
        <cra-icon slot="icon" name="more_vertical"></cra-icon>
      </cra-icon-button>
    </div>`;
  }

  private groupRecordings(recordings: RecordingMetadata[]) {
    recordings = recordings.sort((a, b) => b.recordedAt - a.recordedAt);
    const today = getToday();
    const yesterday = getYesterday();
    const todayLabel = i18n.recordingListTodayHeader;
    const yesterdayLabel = i18n.recordingListYesterdayHeader;
    const thisMonthLabel = i18n.recordingListThisMonthHeader;

    const groups: RecordingGroup[] = [];
    let currentGroup:
      RecordingGroup = {sectionLabel: todayLabel, recordings: []};

    for (const recording of recordings) {
      const recordedAt = recording.recordedAt;
      let dateGroup = getMonthLabel(recordedAt);
      if (recordedAt >= today) {
        dateGroup = todayLabel;
      } else if (recordedAt >= yesterday && recordedAt < today) {
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
          recordings: [recording],
        };
      } else {
        currentGroup.recordings.push(recording);
      }
    }
    if (currentGroup.recordings.length > 0) {
      groups.push(currentGroup);
    }
    return groups;
  }

  private renderRecordingList() {
    // Sort most recent recordings first.
    if (settings.value.recordingSortType === RecordingSortType.DATE) {
      const recordings = Object.values(this.recordingMetadataMap);
      const recordingGroups = this.groupRecordings(recordings);

      return map(recordingGroups, (group) => html`
        <div class="section-heading">${group.sectionLabel}</div>
        ${map(group.recordings, (rec) => this.renderRecording(rec))}
      `);
    }

    // Sort by recording titles ascendingly (A to Z).
    if (settings.value.recordingSortType === RecordingSortType.NAME) {
      const recordings = Object.values(this.recordingMetadataMap
      ).sort((a, b) => a.title.localeCompare(b.title));
      return map(recordings, (rec) => this.renderRecording(rec));
    }

    assertExhaustive(settings.value.recordingSortType);
  }

  override render(): RenderResult {
    if (Object.keys(this.recordingMetadataMap).length === 0) {
      // TODO: b/336963138 - Add a placeholder illustration and add the
      // illustration for no recording when it's ready.
      return html`No recording`;
    }
    return [
      this.renderHeader(),
      html`<div id="list">${this.renderRecordingList()}</div>`,
    ];
  }
}

window.customElements.define('recording-file-list', RecordingFileList);

declare global {
  interface HTMLElementTagNameMap {
    'recording-file-list': RecordingFileList;
  }
}
