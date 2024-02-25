// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DetailsProviderRemote} from './discards.mojom-webui.js';
import {DetailsProvider} from './discards.mojom-webui.js';
import type {SiteDataProviderRemote} from './site_data.mojom-webui.js';
import {SiteDataProvider} from './site_data.mojom-webui.js';

// The following variables are initialized by 'initialize'.
// Points to the DiscardsDetailsProviderRemote.
let discardsDetailsProvider: DetailsProviderRemote;

/**
 * @return Provider of discards details.
 */
export function getOrCreateDetailsProvider(): DetailsProviderRemote {
  if (!discardsDetailsProvider) {
    discardsDetailsProvider = DetailsProvider.getRemote();
  }
  return discardsDetailsProvider;
}

let siteDataProvider: SiteDataProviderRemote;

/** @return Provider of site data info */
export function getOrCreateSiteDataProvider(): SiteDataProviderRemote {
  if (!siteDataProvider) {
    siteDataProvider = SiteDataProvider.getRemote();
  }
  return siteDataProvider;
}

/**
 * Pluralizes a string according to the given count. Assumes that appending an
 * 's' is sufficient to make a string plural.
 * @param s The string to be made plural if necessary.
 * @param n The count of the number of objects.
 * @return The plural version of |s| if n !== 1, otherwise |s|.
 */
export function maybeMakePlural(s: string, n: number): string {
  return n === 1 ? s : s + 's';
}

/**
 * Converts a |seconds| interval to a user friendly string.
 * @param seconds The interval to render.
 * @return An English string representing the interval.
 */
export function secondsToString(seconds: number): string {
  // These constants aren't perfect, but close enough.
  const SECONDS_PER_MINUTE = 60;
  const MINUTES_PER_HOUR = 60;
  const SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
  const HOURS_PER_DAY = 24;
  const SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;
  const DAYS_PER_WEEK = 7;
  const SECONDS_PER_WEEK = SECONDS_PER_DAY * DAYS_PER_WEEK;
  const SECONDS_PER_MONTH = SECONDS_PER_DAY * 30.5;
  const SECONDS_PER_YEAR = SECONDS_PER_DAY * 365;

  // Seconds.
  if (seconds < SECONDS_PER_MINUTE) {
    return seconds.toString() + maybeMakePlural(' second', seconds);
  }

  // Minutes.
  let minutes = Math.floor(seconds / SECONDS_PER_MINUTE);
  if (minutes < MINUTES_PER_HOUR) {
    return minutes.toString() + maybeMakePlural(' minute', minutes);
  }

  // Hours and minutes.
  const hours = Math.floor(seconds / SECONDS_PER_HOUR);
  minutes = minutes % MINUTES_PER_HOUR;
  if (hours < HOURS_PER_DAY) {
    let s = hours.toString() + maybeMakePlural(' hour', hours);
    if (minutes > 0) {
      s += ' and ' + minutes.toString() + maybeMakePlural(' minute', minutes);
    }
    return s;
  }

  // Days.
  const days = Math.floor(seconds / SECONDS_PER_DAY);
  if (days < DAYS_PER_WEEK) {
    return days.toString() + maybeMakePlural(' day', days);
  }

  // Weeks. There's an awkward gap to bridge where 4 weeks can have
  // elapsed but not quite 1 month. Be sure to use weeks to report that.
  const weeks = Math.floor(seconds / SECONDS_PER_WEEK);
  const months = Math.floor(seconds / SECONDS_PER_MONTH);
  if (months < 1) {
    return 'over ' + weeks.toString() + maybeMakePlural(' week', weeks);
  }

  // Months.
  const years = Math.floor(seconds / SECONDS_PER_YEAR);
  if (years < 1) {
    return 'over ' + months.toString() + maybeMakePlural(' month', months);
  }

  // Years.
  return 'over ' + years.toString() + maybeMakePlural(' year', years);
}

/**
 * Converts a |secondsAgo| duration to a user friendly string.
 * @param secondsAgo The duration to render.
 * @return An English string representing the duration.
 */
export function durationToString(secondsAgo: number): string {
  const ret = secondsToString(secondsAgo);

  if (ret.endsWith(' seconds') || ret.endsWith(' second')) {
    return 'just now';
  }

  return ret + ' ago';
}

/**
 * Returns a string representation of a boolean value for display in a table.
 * @param bool A boolean value.
 * @return A string representing the bool.
 */
export function boolToString(bool: boolean): string {
  return bool ? '✔' : '✘️';
}
