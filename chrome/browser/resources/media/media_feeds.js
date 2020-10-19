// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Allow a function to be provided by tests, which will be called when
// the page has been populated with media feeds details.
const mediaFeedsPageIsPopulatedResolver = new PromiseResolver();
function whenPageIsPopulatedForTest() {
  return mediaFeedsPageIsPopulatedResolver.promise;
}

const mediaFeedItemsPageIsPopulatedResolver = new PromiseResolver();
function whenFeedTableIsPopulatedForTest() {
  return mediaFeedItemsPageIsPopulatedResolver.promise;
}

const mediaFeedsConfigTableIsPopulatedResolver = new PromiseResolver();
function whenConfigTableIsPopulatedForTest() {
  return mediaFeedsConfigTableIsPopulatedResolver.promise;
}

const mediaFeedsConfigTableSafeSearchPrefIsUpdatedResolver =
    new PromiseResolver();
function whenConfigTableSafeSearchPrefIsUpdatedForTest() {
  return mediaFeedsConfigTableSafeSearchPrefIsUpdatedResolver.promise;
}

const mediaFeedsConfigTableBackgroundFetchingPrefIsUpdatedResolver =
    new PromiseResolver();
function whenConfigTableBackgroundFetchingPrefIsUpdatedForTest() {
  return mediaFeedsConfigTableBackgroundFetchingPrefIsUpdatedResolver.promise;
}

(function() {

let delegate = null;
let feedsTable = null;
let feedItemsTable = null;
let store = null;
let configTableBody = null;

/** @implements {cr.ui.MediaDataTableDelegate} */
class MediaFeedsTableDelegate {
  /**
   * Formats a field to be displayed in the data table and inserts it into the
   * element.
   * @param {Element} td
   * @param {?Object} data
   * @param {string} key
   * @param {Object} dataRow
   */
  insertDataField(td, data, key, dataRow) {
    if (key == 'actions') {
      const a = document.createElement('a');
      a.href = '#feed-content';
      a.textContent = 'Show Contents';
      td.appendChild(a);

      a.addEventListener('click', () => {
        showFeedContents(dataRow);

        // Clear old logs and hide the area from display.
        $('fetch-logs').style.display = 'none';
        $('fetch-logs-content').textContent = '';
      });

      td.appendChild(document.createElement('br'));

      const fetchFeed = document.createElement('a');
      fetchFeed.href = '#feed-content';
      fetchFeed.textContent = 'Fetch Feed';
      td.appendChild(fetchFeed);

      fetchFeed.addEventListener('click', () => {
        store.fetchMediaFeed(dataRow.id).then(response => {
          updateFeedsTable();
          showFeedContents(dataRow);

          $('fetch-logs').style.display = 'block';
          $('fetch-logs-content').textContent = response.logs;
        });
      });
    }

    if (data === undefined || data === null) {
      return;
    }

    if (key === 'url') {
      // Format a mojo GURL.
      td.textContent = data.url;
    } else if (
        key === 'lastDiscoveryTime' || key === 'lastFetchTime' ||
        key === 'lastFetchTimeNotCacheHit' || key === 'datePublished' ||
        key === 'lastDisplayTime') {
      // Format a mojo time.
      td.textContent =
          convertMojoTimeToJS(/** @type {mojoBase.mojom.Time} */ (data))
              .toLocaleString();
    } else if (key === 'userStatus') {
      // Format a FeedUserStatus.
      if (data == mediaFeeds.mojom.FeedUserStatus.kAuto) {
        td.textContent = 'Auto';
      } else if (data == mediaFeeds.mojom.FeedUserStatus.kDisabled) {
        td.textContent = 'Disabled';
      } else if (data == mediaFeeds.mojom.FeedUserStatus.kEnabled) {
        td.textContent = 'Enabled';
      }
    } else if (key === 'lastFetchResult') {
      // Format a FetchResult.
      if (data == mediaFeeds.mojom.FetchResult.kNone) {
        td.textContent = 'None';
      } else if (data == mediaFeeds.mojom.FetchResult.kSuccess) {
        td.textContent = 'Success';
      } else if (data == mediaFeeds.mojom.FetchResult.kFailedBackendError) {
        td.textContent = 'Failed (Backend Error)';
      } else if (data == mediaFeeds.mojom.FetchResult.kFailedNetworkError) {
        td.textContent = 'Failed (Network Error)';
      }
    } else if (key === 'lastFetchContentTypes') {
      // Format a MediaFeedItemType.
      const contentTypes = [];
      const itemType = parseInt(data, 10);

      if (itemType & mediaFeeds.mojom.MediaFeedItemType.kVideo) {
        contentTypes.push('Video');
      } else if (itemType & mediaFeeds.mojom.MediaFeedItemType.kTVSeries) {
        contentTypes.push('TV Series');
      } else if (itemType & mediaFeeds.mojom.MediaFeedItemType.kMovie) {
        contentTypes.push('Movie');
      }

      td.textContent =
          contentTypes.length === 0 ? 'None' : contentTypes.join(',');
    } else if (key === 'logos' || key === 'images') {
      // Format an array of mojo media images.
      // TODO(crbug.com/1074478): Display the image content attributes.
      data.forEach((image) => {
        const a = document.createElement('a');
        a.href = image.src.url;
        a.textContent = image.src.url;
        a.target = '_blank';
        td.appendChild(a);
        td.appendChild(document.createElement('br'));

        const contentAttributes = [];
        if (image.contentAttributes && image.contentAttributes.length !== 0) {
          const p = document.createElement('p');
          const contentAttributes = [];

          image.contentAttributes.forEach((contentAttribute) => {
            contentAttributes.push(formatContentAttribute(contentAttribute));
          });

          p.textContent = 'ContentAttributes=' + contentAttributes.join(', ');
          td.appendChild(p);
        }
      });
    } else if (key == 'type') {
      // Format a MediaFeedItemType.
      switch (parseInt(data, 10)) {
        case mediaFeeds.mojom.MediaFeedItemType.kVideo:
          td.textContent = 'Video';
          break;
        case mediaFeeds.mojom.MediaFeedItemType.kTVSeries:
          td.textContent = 'TV Series';
          break;
        case mediaFeeds.mojom.MediaFeedItemType.kMovie:
          td.textContent = 'Movie';
          break;
      }
    } else if (key == 'isFamilyFriendly') {
      // Format a IsFamilyFriendly.
      switch (parseInt(data, 10)) {
        case mediaFeeds.mojom.IsFamilyFriendly.kUnknown:
          td.textContent = 'Unknown';
          break;
        case mediaFeeds.mojom.IsFamilyFriendly.kYes:
          td.textContent = 'Yes';
          break;
        case mediaFeeds.mojom.IsFamilyFriendly.kNo:
          td.textContent = 'No';
          break;
      }
    } else if (key == 'clicked') {
      // Format a boolean.
      td.textContent = data ? 'Yes' : 'No';
    } else if (key == 'actionStatus') {
      // Format a MediaFeedItemActionStatus.
      switch (parseInt(data, 10)) {
        case mediaFeeds.mojom.MediaFeedItemActionStatus.kUnknown:
          td.textContent = 'Unknown';
          break;
        case mediaFeeds.mojom.MediaFeedItemActionStatus.kActive:
          td.textContent = 'Active';
          break;
        case mediaFeeds.mojom.MediaFeedItemActionStatus.kPotential:
          td.textContent = 'Potential';
          break;
        case mediaFeeds.mojom.MediaFeedItemActionStatus.kCompleted:
          td.textContent = 'Completed';
          break;
      }
    } else if (key == 'safeSearchResult') {
      // Format a SafeSearchResult.
      switch (parseInt(data, 10)) {
        case mediaFeeds.mojom.SafeSearchResult.kUnknown:
          td.textContent = 'Unknown';
          break;
        case mediaFeeds.mojom.SafeSearchResult.kSafe:
          td.textContent = 'Safe';
          break;
        case mediaFeeds.mojom.SafeSearchResult.kUnsafe:
          td.textContent = 'Unsafe';
          break;
      }
    } else if (key == 'startTime' || key == 'duration') {
      // Format a start time.
      td.textContent =
          timeDeltaToSeconds(/** @type {mojoBase.mojom.TimeDelta} */ (data));
    } else if (key == 'interactionCounters') {
      // Format interaction counters.
      const counters = [];

      Object.keys(data).forEach((key) => {
        let keyString = '';

        switch (parseInt(key, 10)) {
          case mediaFeeds.mojom.InteractionCounterType.kWatch:
            keyString = 'Watch';
            break;
          case mediaFeeds.mojom.InteractionCounterType.kLike:
            keyString = 'Like';
            break;
          case mediaFeeds.mojom.InteractionCounterType.kDislike:
            keyString = 'Dislike';
            break;
        }

        counters.push(keyString + '=' + data[key]);
      });

      td.textContent = counters.join(' ');
    } else if (key == 'contentRatings') {
      // Format content ratings.
      const ratings = [];

      data.forEach((rating) => {
        ratings.push(rating.agency + ' ' + rating.value);
      });

      td.textContent = ratings.join(', ');
    } else if (key == 'author') {
      // Format a mojom author.
      const a = document.createElement('a');
      a.href = data.url;
      a.textContent = data.name;
      a.target = '_blank';
      td.appendChild(a);
    } else if (key == 'name') {
      // Format a mojo string16.
      td.textContent =
          decodeString16(/** @type {mojoBase.mojom.String16} */ (data));
    } else if (key == 'genre') {
      // Format an array of strings.
      td.textContent = data.join(', ');
    } else if (key == 'live') {
      td.textContent =
          formatLiveDetails(/** @type {mediaFeeds.mojom.LiveDetails} */ (data));
    } else if (key == 'tvEpisode') {
      // Format a TV Episode.
      td.textContent = data.name + ' EpisodeNumber=' + data.episodeNumber +
          ' SeasonNumber=' + data.seasonNumber + ' ' +
          formatIdentifiers(/** @type {Array<mediaFeeds.mojom.Identifier>} */ (
              data.identifiers))  + ' DurationSecs=' +
              timeDeltaToSeconds(data.duration);
      if (data.live) {
        td.textContent +=
            ' LiveDetails=' + formatLiveDetails(
            /** @type {mediaFeeds.mojom.LiveDetails} */ (data.live));
      }
    } else if (key == 'playNextCandidate') {
      // Format a Play Next Candidate.
      td.textContent = data.name + ' EpisodeNumber=' + data.episodeNumber +
          ' SeasonNumber=' + data.seasonNumber + ' ' +
          formatIdentifiers(
                           /** @type {Array<mediaFeeds.mojom.Identifier>} */ (
                               data.identifiers)) +
          ' ActionURL=' + data.action.url.url;

      if (data.action.startTime) {
        td.textContent +=
            ' ActionStartTimeSecs=' + timeDeltaToSeconds(data.action.startTime);
      }

      td.textContent += ' DurationSecs=' + timeDeltaToSeconds(data.duration);
    } else if (key == 'identifiers') {
      // Format identifiers.
      td.textContent = formatIdentifiers(
          /** @type {Array<mediaFeeds.mojom.Identifier>} */ (data));
    } else if (key === 'lastFetchItemCount') {
      // Format the fetch item count.
      td.textContent =
          data + ' (' + dataRow.lastFetchSafeItemCount + ' confirmed as safe)';
    } else if (key == 'resetReason') {
      // Format a ResetReason.
      switch (parseInt(data, 10)) {
        case mediaFeeds.mojom.ResetReason.kNone:
          td.textContent = 'None';
          break;
        case mediaFeeds.mojom.ResetReason.kCookies:
          td.textContent = 'Cookies';
          break;
        case mediaFeeds.mojom.ResetReason.kVisit:
          td.textContent = 'Visit';
          break;
        case mediaFeeds.mojom.ResetReason.kCache:
          td.textContent = 'Cache';
          break;
      }
    } else if (key === 'userIdentifier') {
      if (data) {
        td.textContent = 'Name=' + data.name;

        if (data.email) {
          td.textContent += ' Email=' + data.email;
        }

        if (data.image) {
          td.textContent += ' Image=' + data.image.src.url;
        }
      }
    } else {
      td.textContent = data;
    }
  }

  /**
   * Compares two objects based on |sortKey|.
   * @param {string} sortKey The name of the property to sort by.
   * @param {?Object} a The first object to compare.
   * @param {?Object} b The second object to compare.
   * @return {number} A negative number if |a| should be ordered
   *     before |b|, a positive number otherwise.
   */
  compareTableItem(sortKey, a, b) {
    const val1 = a[sortKey];
    const val2 = b[sortKey];

    if (sortKey === 'url') {
      return val1.url > val2.url ? 1 : -1;
    } else if (
        sortKey === 'id' || sortKey === 'displayName' ||
        sortKey === 'userStatus' || sortKey === 'lastFetchResult' ||
        sortKey === 'fetchFailedCount' || sortKey === 'lastFetchItemCount' ||
        sortKey === 'lastFetchPlayNextCount' ||
        sortKey === 'lastFetchContentTypes' || sortKey === 'safeSearchResult' ||
        sortKey === 'type' || sortKey === 'cookieNameFilter') {
      return val1 > val2 ? 1 : -1;
    } else if (
        sortKey === 'lastDiscoveryTime' || sortKey === 'lastFetchTime' ||
        sortKey === 'lastFetchTimeNotCacheHit' ||
        sortKey === 'lastDisplayTime') {
      return val1.internalValue > val2.internalValue ? 1 : -1;
    }

    assertNotReached('Unsupported sort key: ' + sortKey);
    return 0;
  }
}

/**
 * Convert a time delta to seconds.
 * @param {mojoBase.mojom.TimeDelta} timeDelta
 * @returns {number}
 */
function timeDeltaToSeconds(timeDelta) {
  return timeDelta.microseconds / 1000 / 1000;
}

/**
 * Formats an array of identifiers for display.
 * @param {Array<mediaFeeds.mojom.Identifier>} mojoIdentifiers
 * @returns {string}
 */
function formatIdentifiers(mojoIdentifiers) {
  const identifiers = [];

  mojoIdentifiers.forEach((identifier) => {
    let keyString = '';

    switch (identifier.type) {
      case mediaFeeds.mojom.Identifier_Type.kTMSRootId:
        keyString = 'TMSRootId';
        break;
      case mediaFeeds.mojom.Identifier_Type.kTMSId:
        keyString = 'TMSId';
        break;
      case mediaFeeds.mojom.Identifier_Type.kPartnerId:
        keyString = 'PartnerId';
        break;
    }

    identifiers.push(keyString + '=' + identifier.value);
  });

  return identifiers.join(' ');
}

/**
 * Formats a LiveDetails struct for display.
 * @param {mediaFeeds.mojom.LiveDetails} mojoLiveDetails
 * @returns {string}
 */
function formatLiveDetails(mojoLiveDetails) {
  let textContent = 'Live';

  if (mojoLiveDetails.startTime) {
    textContent += ' ' +
        'StartTime=' +
        convertMojoTimeToJS(
            /** @type {mojoBase.mojom.Time} */ (mojoLiveDetails.startTime))
            .toLocaleString();
  }

  if (mojoLiveDetails.endTime) {
    textContent += ' ' +
        'EndTime=' +
        convertMojoTimeToJS(
            /** @type {mojoBase.mojom.Time} */ (mojoLiveDetails.endTime))
            .toLocaleString();
  }

  return textContent;
}

/**
 * Formats a single ContentAttribute for display.
 * @param {mediaFeeds.mojom.ContentAttribute} contentAttribute
 * @returns {string}
 */
function formatContentAttribute(contentAttribute) {
  switch (parseInt(contentAttribute, 10)) {
    case mediaFeeds.mojom.ContentAttribute.kIconic:
      return 'Iconic';
    case mediaFeeds.mojom.ContentAttribute.kSceneStill:
      return 'SceneStill';
    case mediaFeeds.mojom.ContentAttribute.kPoster:
      return 'Poster';
    case mediaFeeds.mojom.ContentAttribute.kBackground:
      return 'Background';
    case mediaFeeds.mojom.ContentAttribute.kForDarkBackground:
      return 'ForDarkBackground';
    case mediaFeeds.mojom.ContentAttribute.kForLightBackground:
      return 'ForLightBackground';
    case mediaFeeds.mojom.ContentAttribute.kCentered:
      return 'Centered';
    case mediaFeeds.mojom.ContentAttribute.kRightCentered:
      return 'RightCentered';
    case mediaFeeds.mojom.ContentAttribute.kLeftCentered:
      return 'LeftCentered';
    case mediaFeeds.mojom.ContentAttribute.kHasTransparentBackground:
      return 'HasTransparentBackground';
    case mediaFeeds.mojom.ContentAttribute.kHasTitle:
      return 'HasTitle';
    case mediaFeeds.mojom.ContentAttribute.kNoTitle:
      return 'NoTitle';
    default:
      return 'Unknown';
  }
}

/**
 * Parses utf16 coded string.
 * @param {?mojoBase.mojom.String16} arr
 * @return {string}
 */
function decodeString16(arr) {
  if (arr == null) {
    return '';
  }

  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * Converts a mojo time to a JS time.
 * @param {mojoBase.mojom.Time} mojoTime
 * @return {Date}
 */
function convertMojoTimeToJS(mojoTime) {
  if (mojoTime === null) {
    return new Date();
  }

  // The new Date().getTime() returns the number of milliseconds since the
  // UNIX epoch (1970-01-01 00::00:00 UTC), while |internalValue| of the
  // device.mojom.Geoposition represents the value of microseconds since the
  // Windows FILETIME epoch (1601-01-01 00:00:00 UTC). So add the delta when
  // sets the |internalValue|. See more info in //base/time/time.h.
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  // |epochDeltaInMs| equals to base::Time::kTimeTToMicrosecondsOffset.
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const timeInMs = Number(mojoTime.internalValue) / 1000;

  return new Date(timeInMs - epochDeltaInMs);
}

/**
 * Creates a single row in the config table.
 * @param {string} name The name of the config setting.
 * @param {string} value The value of the config setting.
 * @return {!Node}
 */
function createConfigRow(name, value) {
  const tr = document.createElement('tr');

  const nameCell = document.createElement('td');
  nameCell.textContent = name;
  tr.appendChild(nameCell);

  const valueCell = document.createElement('td');
  valueCell.textContent = value;
  tr.appendChild(valueCell);

  return tr;
}

/**
 * Creates a single row in the config table with a toggle button.
 * @param {string} name The name of the config setting.
 * @param {string} value The value of the config setting.
 * @param {Function} clickAction The function to be called to toggle the row.
 * @return {!Node}
 */
function createConfigRowWithToggle(name, value, clickAction) {
  const tr = document.createElement('tr');

  const nameCell = document.createElement('td');
  nameCell.textContent = name;
  tr.appendChild(nameCell);

  const valueCell = document.createElement('td');
  tr.appendChild(valueCell);

  const a = document.createElement('a');
  a.href = '#';
  a.textContent = value + ' (Toggle)';
  a.addEventListener('click', clickAction);
  valueCell.appendChild(a);

  return tr;
}

/**
 * Regenerates the config table.
 * @param {!mediaFeeds.mojom.DebugInformation} info The debug info
 */
function renderConfigTable(info) {
  configTableBody.innerHTML = trustedTypes.emptyHTML;

  configTableBody.appendChild(createConfigRow(
      'Safe Search Enabled (value)',
      formatFeatureFlag(info.safeSearchFeatureEnabled)));

  configTableBody.appendChild(createConfigRowWithToggle(
      'Safe Search Enabled (pref)', formatFeatureFlag(info.safeSearchPrefValue),
      () => {
        store.setSafeSearchEnabledPref(!info.safeSearchPrefValue).then(() => {
          updateConfigTable().then(
              () => mediaFeedsConfigTableSafeSearchPrefIsUpdatedResolver
                        .resolve());
        });
      }));

  configTableBody.appendChild(createConfigRow(
      'Background Fetching Enabled (value)',
      formatFeatureFlag(info.backgroundFetchingFeatureEnabled)));

  configTableBody.appendChild(createConfigRowWithToggle(
      'Background Fetching Enabled (pref)',
      formatFeatureFlag(info.backgroundFetchingPrefValue), () => {
        store.setBackgroundFetchingPref(!info.backgroundFetchingPrefValue)
            .then(() => {
              updateConfigTable().then(
                  () =>
                      mediaFeedsConfigTableBackgroundFetchingPrefIsUpdatedResolver
                          .resolve());
            });
      }));
}

/**
 * Retrieve debug info and render the config table.
 */
function updateConfigTable() {
  return store.getDebugInformation().then(response => {
    renderConfigTable(response.info);
    mediaFeedsConfigTableIsPopulatedResolver.resolve();
  });
}

/**
 * Converts a boolean into a string value.
 * @param {boolean} value The value of the config setting.
 * @return {string}
 */
function formatFeatureFlag(value) {
  return value ? 'Enabled' : 'Disabled';
}

/**
 * Retrieve feed info and render the feed table.
 */
function updateFeedsTable() {
  store.getMediaFeeds().then(response => {
    feedsTable.setData(response.feeds);
    mediaFeedsPageIsPopulatedResolver.resolve();
  });
}

/**
 * Retrieve feed items and render the feed contents table.
 * @param {Object} dataRow
 */
function showFeedContents(dataRow) {
  store.getItemsForMediaFeed(dataRow.id).then(response => {
    feedItemsTable.setData(response.items);

    // Show the feed items section.
    $('current-feed').textContent = dataRow.url.url;
    $('feed-content').style.display = 'block';

    mediaFeedItemsPageIsPopulatedResolver.resolve();
  });
}

document.addEventListener('DOMContentLoaded', () => {
  store = mediaFeeds.mojom.MediaFeedsStore.getRemote();

  configTableBody = $('config-table-body');
  updateConfigTable();

  delegate = new MediaFeedsTableDelegate();
  feedsTable = new cr.ui.MediaDataTable($('feeds-table'), delegate);
  feedItemsTable = new cr.ui.MediaDataTable($('feed-items-table'), delegate);

  updateFeedsTable();

  // Add handler to 'copy all to clipboard' button
  const copyAllToClipboardButton = $('copy-all-to-clipboard');
  copyAllToClipboardButton.addEventListener('click', (e) => {
    // Make sure nothing is selected
    window.getSelection().removeAllRanges();

    document.execCommand('selectAll');
    document.execCommand('copy');

    // And deselect everything at the end.
    window.getSelection().removeAllRanges();
  });
});
})();
