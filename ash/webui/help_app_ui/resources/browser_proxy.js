// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {PageHandlerFactory, PageHandlerRemote} from './help_app_ui.mojom-webui.js';
import {Index, IndexRemote} from './index.mojom-webui.js';
import {MessagePipe} from './message_pipe.js';
import {Message} from './message_types.js';
import {SearchConcept, SearchHandler, SearchHandlerRemote} from './search.mojom-webui.js';
import {Content, ResponseStatus, Result} from './types.mojom-webui.js';

const help_app = {
  handler: new PageHandlerRemote(),
};

// Set up a page handler to talk to the browser process.
PageHandlerFactory.getRemote().createPageHandler(
    help_app.handler.$.bindNewPipeAndPassReceiver());

// Set up an index remote to talk to Local Search Service.
/** @type {!IndexRemote} */
const indexRemote = Index.getRemote();

// Expose `indexRemote` on `window`, because it is accessed by a CrOS Tast test.
Object.assign(window, {indexRemote});

/**
 * Talks to the search handler. Use for updating the content for launcher
 * search.
 *
 * @type {!SearchHandlerRemote}
 */
const searchHandlerRemote = SearchHandler.getRemote();

const GUEST_ORIGIN = 'chrome-untrusted://help-app';
const MAX_STRING_LEN = 9999;
const guestFrame =
    /** @type {!HTMLIFrameElement} */ (document.createElement('iframe'));
guestFrame.src = `${GUEST_ORIGIN}${location.pathname}`;
document.body.appendChild(guestFrame);

// Cached result whether Local Search Service is enabled.
/** @type {Promise<boolean>} */
const isLssEnabled =
    help_app.handler.isLssEnabled().then(result => result.enabled);

// Cached result of whether Launcher Search is enabled.
/** @type {Promise<boolean>} */
const isLauncherSearchEnabled =
    help_app.handler.isLauncherSearchEnabled().then(result => result.enabled);

/**
 * @param {string|Object} url
 * @return {!Url}
 */
function toUrl(url) {
  // TODO(b/279132899): Figure out why `url` is an empty object when it should
  // have been an empty string.
  if (url === '' || typeof (url) !== 'string') {
    return /** @type {!Url} */ ({url: ''});
  }
  return /** @type {!Url} */ ({url});
}

/**
 * @param {string} s
 * @return {!String16}
 */
function toTruncatedString16(s) {
  return /** @type {!String16} */ (stringToMojoString16(truncate(s)));
}
const TITLE_ID = 'title';
const BODY_ID = 'body';
const CATEGORY_ID = 'main-category';
const SUBCATEGORY_ID = 'subcategory';
const SUBHEADING_ID = 'subheading';

/**
 * A pipe through which we can send messages to the guest frame.
 * Use an undefined `target` to find the <iframe> automatically.
 * Do not rethrow errors, since handlers installed here are expected to
 * throw exceptions that are handled on the other side of the pipe (in the guest
 * frame), not on this side.
 */
const guestMessagePipe = new MessagePipe(
    'chrome-untrusted://help-app', /*target=*/ undefined,
    /*rethrowErrors=*/ false);

guestMessagePipe.registerHandler(Message.OPEN_FEEDBACK_DIALOG, () => {
  return help_app.handler.openFeedbackDialog();
});

guestMessagePipe.registerHandler(Message.SHOW_PARENTAL_CONTROLS, () => {
  help_app.handler.showParentalControls();
});

guestMessagePipe.registerHandler(
    Message.TRIGGER_WELCOME_TIP_CALL_TO_ACTION, (actionTypeId) => {
      help_app.handler.triggerWelcomeTipCallToAction(actionTypeId);
    });

guestMessagePipe.registerHandler(
    Message.ADD_OR_UPDATE_SEARCH_INDEX, async (message) => {
      if (!(await isLssEnabled)) {
        return;
      }
      const data_from_app =
          /** @type {!Array<!helpApp.SearchableItem>} */ (message);
      const data_to_send = data_from_app.map(searchable_item => {
        /** @type {!Array<!Content>} */
        const contents = [
          {
            id: TITLE_ID,
            content: toTruncatedString16(searchable_item.title),
            weight: 1.0,
          },
          {
            id: CATEGORY_ID,
            content: toTruncatedString16(searchable_item.mainCategoryName),
            weight: 0.1,
          },
        ];
        if (searchable_item.subcategoryNames) {
          for (let i = 0; i < searchable_item.subcategoryNames.length; ++i) {
            contents.push({
              id: SUBCATEGORY_ID + i,
              content: toTruncatedString16(searchable_item.subcategoryNames[i]),
              weight: 0.1,
            });
          }
        }
        // If there are subheadings, use those instead of the body.
        if (searchable_item.subheadings) {
          for (let i = 0; i < searchable_item.subheadings.length; ++i) {
            contents.push({
              id: SUBHEADING_ID + i,
              content: toTruncatedString16(searchable_item.subheadings[i]),
              weight: 0.4,
            });
          }
        } else if (searchable_item.body) {
          contents.push({
            id: BODY_ID,
            content: toTruncatedString16(searchable_item.body),
            weight: 0.2,
          });
        }
        return {
          id: searchable_item.id,
          contents,
          locale: searchable_item.locale,
        };
      });
      return indexRemote.addOrUpdate(data_to_send);
    });

guestMessagePipe.registerHandler(Message.CLEAR_SEARCH_INDEX, async () => {
  if (!(await isLssEnabled)) {
    return;
  }
  return indexRemote.clearIndex();
});

guestMessagePipe.registerHandler(
    Message.FIND_IN_SEARCH_INDEX, async (message) => {
      if (!(await isLssEnabled)) {
        return {results: null};
      }
      const dataFromApp =
          /** @type {{query: string, maxResults:(number|undefined)}} */
          (message);
      const response = await indexRemote.find(
          toTruncatedString16(dataFromApp.query), dataFromApp.maxResults || 50);

      if (response.status !== ResponseStatus.kSuccess || !response.results) {
        return {results: null};
      }
      const search_results =
          /** @type {!Array<!Result>} */ (response.results);
      // Sort results by decreasing score.
      search_results.sort((a, b) => b.score - a.score);
      /** @type {!Array<!helpApp.SearchResult>} */
      const results = search_results.map(result => {
        /** @type {!Array<!helpApp.Position>} */
        const titlePositions = [];
        /** @type {!Array<!helpApp.Position>} */
        const bodyPositions = [];
        // Id of the best subheading that appears in positions. We consider
        // the subheading containing the most match positions to be the best.
        // "" means no subheading positions found.
        let bestSubheadingId = '';
        /**
         * Counts how many positions there are for each subheading id.
         * @type {!Object<string, number>}
         */
        const subheadingPosCounts = {};
        // Note: result.positions is not sorted.
        for (const position of result.positions) {
          if (position.contentId === TITLE_ID) {
            titlePositions.push(
                {length: position.length, start: position.start});
          } else if (position.contentId === BODY_ID) {
            bodyPositions.push(
                {length: position.length, start: position.start});
          } else if (position.contentId.startsWith(SUBHEADING_ID)) {
            // Update the subheadings's position count and check if it's the new
            // best subheading.
            const newCount = (subheadingPosCounts[position.contentId] || 0) + 1;
            subheadingPosCounts[position.contentId] = newCount;
            if (!bestSubheadingId ||
                newCount > subheadingPosCounts[bestSubheadingId]) {
              bestSubheadingId = position.contentId;
            }
          }
        }
        // Use only the positions of the best subheading.
        /** @type {!Array<!helpApp.Position>} */
        const subheadingPositions = [];
        if (bestSubheadingId) {
          for (const position of result.positions) {
            if (position.contentId === bestSubheadingId) {
              subheadingPositions.push({
                start: position.start,
                length: position.length,
              });
            }
          }
          subheadingPositions.sort(compareByStart);
        }

        // Sort positions by start index.
        titlePositions.sort(compareByStart);
        bodyPositions.sort(compareByStart);
        return {
          id: result.id,
          titlePositions,
          bodyPositions,
          subheadingIndex: bestSubheadingId ?
              Number(bestSubheadingId.substring(SUBHEADING_ID.length)) :
              null,
          subheadingPositions: bestSubheadingId ? subheadingPositions : null,
        };
      });
      return {results};
    });

guestMessagePipe.registerHandler(Message.CLOSE_BACKGROUND_PAGE, async () => {
  // TODO(b/186180962): Add background page and test that it closes when done.
  if (window.location.pathname !== '/background') {
    return;
  }
  window.close();
  return;
});

guestMessagePipe.registerHandler(
    Message.UPDATE_LAUNCHER_SEARCH_INDEX, async (message) => {
      if (!(await isLauncherSearchEnabled)) {
        return;
      }

      const dataFromApp =
          /** @type {!Array<!helpApp.LauncherSearchableItem>} */ (message);
      /** @type {!Array<!SearchConcept>} */
      const dataToSend = dataFromApp.map(
          searchableItem => ({
            id: truncate(searchableItem.id),
            title: toTruncatedString16(searchableItem.title),
            mainCategory: toTruncatedString16(searchableItem.mainCategoryName),
            tags: searchableItem.tags.map(tag => toTruncatedString16(tag))
                      .filter(tag => tag.data.length > 0),
            tagLocale: searchableItem.tagLocale || '',
            urlPathWithParameters:
                truncate(searchableItem.urlPathWithParameters),
            locale: truncate(searchableItem.locale),
          }));
      // Filter out invalid items. No field can be empty except locale.
      const dataFiltered = dataToSend.filter(item => {
        const valid = item.id && item.title && item.mainCategory &&
            item.tags.length > 0 && item.urlPathWithParameters;
        // This is a google-internal histogram. If changing this, also change
        // the corresponding histograms file.
        if (!valid) {
          chrome.metricsPrivate.recordSparseValueWithPersistentHash(
              'Discover.LauncherSearch.InvalidConceptInUpdate', item.id);
        }
        return valid;
      });
      return searchHandlerRemote.update(dataFiltered);
    });

guestMessagePipe.registerHandler(Message.LAUNCH_MICROSOFT_365_SETUP, () => {
  help_app.handler.launchMicrosoft365Setup();
});

guestMessagePipe.registerHandler(
    Message.MAYBE_SHOW_DISCOVER_NOTIFICATION, () => {
      help_app.handler.maybeShowDiscoverNotification();
    });

guestMessagePipe.registerHandler(
    Message.MAYBE_SHOW_RELEASE_NOTES_NOTIFICATION, () => {
      help_app.handler.maybeShowReleaseNotesNotification();
    });

guestMessagePipe.registerHandler(Message.GET_DEVICE_INFO, async () => {
  return (await help_app.handler.getDeviceInfo()).deviceInfo;
});

guestMessagePipe.registerHandler(
    Message.OPEN_URL_IN_BROWSER_AND_TRIGGER_INSTALL_DIALOG, (url) => {
      help_app.handler.openUrlInBrowserAndTriggerInstallDialog(toUrl(url));
    });

/**
 * Compare two positions by their start index. Use for sorting.
 *
 * @param {!helpApp.Position} a
 * @param {!helpApp.Position} b
 */
function compareByStart(a, b) {
  return a.start - b.start;
}

/**
 * Limits the maximum length of the input string. Converts non-strings into
 * empty string.
 *
 * @param {*} s Probably a string, but might not be.
 * @return {string}
 */
function truncate(s) {
  if (typeof s !== 'string') {
    return '';
  }
  if (s.length <= MAX_STRING_LEN) {
    return s;
  }
  return s.substring(0, MAX_STRING_LEN);
}

export const TEST_ONLY = {guestMessagePipe};
