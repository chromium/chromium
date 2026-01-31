// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import type {CommonUpdateOutcome, EventType, HistoryEvent, MergedHistoryEvent, MergedUpdateEvent, Scope, UpdaterProcessMap} from '../event_history.js';
import {getAppId, isMergedHistoryEvent} from '../event_history.js';
import {loadTimeData} from '../i18n_setup.js';
import {getKnownApps} from '../known_apps.js';

/**
 * The filter settings for the event list.
 */
export interface FilterSettings {
  apps: Set<string>;
  eventTypes: Set<EventType>;
  updateOutcomes: Set<CommonUpdateOutcome>;
  startDate: Date|null;
  endDate: Date|null;
  scopes: Set<Scope>;
}

/**
 * Produce a filter settings object populated with reasonable defaults. Some
 * values, e.g. the set of default app filters, are browser-instructed.
 */
export function createDefaultFilterSettings(): FilterSettings {
  const defaultApps = loadTimeData.getString('defaultAppFilters');
  return {
    apps: new Set(defaultApps === '' ? [] : defaultApps.split(',')),
    eventTypes: new Set<EventType>([
      'INSTALL',
      'UPDATE',
      'UNINSTALL',
    ]),
    updateOutcomes: new Set<CommonUpdateOutcome>(['UPDATED', 'UPDATE_ERROR']),
    startDate: null,
    endDate: null,
    scopes: new Set(),
  };
}

/**
 * Produce a filter settings object with no active filters.
 */
export function createEmptyFilterSettings(): FilterSettings {
  return {
    apps: new Set(),
    eventTypes: new Set(),
    updateOutcomes: new Set(),
    startDate: null,
    endDate: null,
    scopes: new Set(),
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
    if (filterSettings.eventTypes.size > 0 &&
        !filterSettings.eventTypes.has(eventType)) {
      return false;
    }
    if (eventType === 'UPDATE' && isMergedHistoryEvent(event)) {
      const updateEvent = event as MergedUpdateEvent;
      const outcome = updateEvent.endEvent.outcome;
      if (outcome !== undefined && filterSettings.updateOutcomes.size > 0 &&
          !(filterSettings.updateOutcomes as ReadonlySet<string>)
               .has(outcome)) {
        return false;
      }
    }
    const date = processMap.eventDate(event);
    if (filterSettings.startDate &&
        (!date || date < filterSettings.startDate)) {
      return false;
    }
    if (filterSettings.endDate && (!date || date > filterSettings.endDate)) {
      return false;
    }
    const process = processMap.getUpdaterProcessForEvent(event);
    assert(process !== undefined);
    if (filterSettings.scopes.size > 0 &&
        (!process.startEvent.scope ||
         !filterSettings.scopes.has(process.startEvent.scope))) {
      return false;
    }
    if (filterSettings.apps.size > 0) {
      const appId = getAppId(event);
      if (!appId) {
        return false;
      }
      const knownAppIdsByName = getKnownApps();
      const matchesFilter =
          Array.from(filterSettings.apps)
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
