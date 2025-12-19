// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CommonUpdateOutcome, EventType, HistoryEvent, MergedHistoryEvent, MergedUpdateEvent, UpdaterProcessMap} from '../event_history.js';
import {getAppId, isMergedHistoryEvent} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';
import {getKnownApps} from '../known_apps.js';

/**
 * The filter settings for the event list.
 */
export interface FilterSettings {
  activeAppFilters: Set<string>;
  activeEventTypeFilters: Set<EventType>;
  activeUpdateOutcomeFilters: Set<CommonUpdateOutcome>;
  startDateFilter: Date|null;
  endDateFilter: Date|null;
}

/**
 * Produce a filter settings object populated with reasonable defaults. Some
 * values, e.g. the set of default app filters, are browser-instructed.
 */
export function createDefaultFilterSettings(): FilterSettings {
  const defaultApps = loadTimeData.getString('defaultAppFilters');
  return {
    activeAppFilters: new Set(defaultApps === '' ? [] : defaultApps.split(',')),
    activeEventTypeFilters: new Set<EventType>([
      'INSTALL',
      'UPDATE',
      'UNINSTALL',
    ]),
    activeUpdateOutcomeFilters:
        new Set<CommonUpdateOutcome>(['UPDATED', 'UPDATE_ERROR']),
    startDateFilter: null,
    endDateFilter: null,
  };
}

/**
 * Produce a filter settings object with no active filters.
 */
export function createEmptyFilterSettings(): FilterSettings {
  return {
    activeAppFilters: new Set(),
    activeEventTypeFilters: new Set(),
    activeUpdateOutcomeFilters: new Set(),
    startDateFilter: null,
    endDateFilter: null,
  };
}

/**
 * Apply filters to a series of events while preserving order.
 */
export function applyFilterSettings(
    processMap: UpdaterProcessMap|undefined,
    events: Array<HistoryEvent|MergedHistoryEvent>,
    filterSettings: FilterSettings): Array<HistoryEvent|MergedHistoryEvent> {
  if (processMap === undefined) {
    return [];
  }
  return events.filter(event => {
    const eventType = isMergedHistoryEvent(event) ? event.startEvent.eventType :
                                                    event.eventType;
    if (filterSettings.activeEventTypeFilters.size > 0 &&
        !filterSettings.activeEventTypeFilters.has(eventType)) {
      return false;
    }
    if (eventType === 'UPDATE' && isMergedHistoryEvent(event)) {
      const updateEvent = event as MergedUpdateEvent;
      const outcome = updateEvent.endEvent.outcome;
      if (outcome !== undefined &&
          filterSettings.activeUpdateOutcomeFilters.size > 0 &&
          !(filterSettings.activeUpdateOutcomeFilters as ReadonlySet<string>)
               .has(outcome)) {
        return false;
      }
    }
    const date = processMap.eventDate(event);
    if (filterSettings.startDateFilter &&
        (!date || date < filterSettings.startDateFilter)) {
      return false;
    }
    if (filterSettings.endDateFilter &&
        (!date || date > filterSettings.endDateFilter)) {
      return false;
    }
    if (filterSettings.activeAppFilters.size > 0) {
      const appId = getAppId(event);
      if (!appId) {
        return false;
      }
      const knownAppIdsByName = getKnownApps();
      const matchesFilter =
          Array.from(filterSettings.activeAppFilters)
              .some(
                  filter => (knownAppIdsByName.has(filter) &&
                             knownAppIdsByName.get(filter)!.includes(appId)) ||
                      (!knownAppIdsByName.has(filter) &&
                       appId === filter.toLowerCase()));
      if (!matchesFilter) {
        return false;
      }
    }
    return true;
  });
}
