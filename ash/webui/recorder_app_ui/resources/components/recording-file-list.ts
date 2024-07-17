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
import './recording-file-list-item.js';

import {Menu} from 'chrome://resources/cros_components/menu/menu.js';
import {
  createRef,
  css,
  html,
  PropertyDeclarations,
  ref,
  repeat,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {
  RecordingMetadata,
  RecordingMetadataMap,
} from '../core/recording_data_manager.js';
import {RecordingSortType, settings} from '../core/state/settings.js';
import {assertExhaustive} from '../core/utils/assert.js';
import {
  getMonthLabel,
  getToday,
  getYesterday,
  isInThisMonth,
} from '../core/utils/datetime.js';

interface RecordingGroup {
  recordings: RecordingMetadata[];
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
  recording: RecordingMetadata,
});

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

  // TODO: b/336963138 - Add back action menu for delete.
  // private onDeleteRecordingClick(id: string) {
  //   this.dispatchEvent(
  //       new CustomEvent('delete-recording-clicked', {detail: id}),
  //   );
  // }

  private onSortingTypeClick(newSortType: RecordingSortType) {
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
      this.onSortingTypeClick(RecordingSortType.DATE);
    }}
      >
      </cros-menu-item>
      <cros-menu-item
        headline=${i18n.recordingListSortByNameOption}
        ?checked=${settings.value.recordingSortType === RecordingSortType.NAME}
        @cros-menu-item-triggered=${() => {
      this.onSortingTypeClick(RecordingSortType.NAME);
    }}
      >
      </cros-menu-item>
    </cra-menu>`;
  }

  private renderHeader() {
    return html`<div id="header">
        <span>${i18n.recordingListHeader}</span>
        <cra-icon-button buttonstyle="floating">
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

  private groupRecordings(recordings: RecordingMetadata[]) {
    recordings = recordings.toSorted((a, b) => b.recordedAt - a.recordedAt);
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

    for (const recording of recordings) {
      const recordedAt = recording.recordedAt;
      let dateGroup = getMonthLabel(recordedAt);
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

  private getRenderRecordingItems(): RenderRecordingItem[] {
    function recordingToRenderRecordingItem(
      recording: RecordingMetadata,
    ): RenderRecordingItem {
      return {
        id: `record-${recording.id}`,
        kind: 'recording',
        recording,
      };
    }

    // Sort most recent recordings first.
    if (settings.value.recordingSortType === RecordingSortType.DATE) {
      const recordings = Object.values(this.recordingMetadataMap);
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
      const recordings = Object.values(this.recordingMetadataMap)
                           .sort(
                             (a, b) => a.title.localeCompare(b.title),
                           );
      return recordings.map(
        (recording) => recordingToRenderRecordingItem(recording),
      );
    }

    assertExhaustive(settings.value.recordingSortType);
  }

  private renderRecordingList() {
    return repeat(
      this.getRenderRecordingItems(),
      (item) => item.id,
      (item) => {
        switch (item.kind) {
          case 'header':
            return html`<div class="section-heading">${item.label}</div>`;
          case 'recording':
            return html`
              <recording-file-list-item .recording=${item.recording}>
              </recording-file-list-item>
            `;
          default:
            assertExhaustive(item);
        }
      },
    );
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
