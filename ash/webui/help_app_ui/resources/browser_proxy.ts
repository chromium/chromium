// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {stringToMojoString16} from './mojo_type_util.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {PageHandlerFactory, PageHandlerRemote} from './help_app_ui.mojom-webui.js';
import {Index} from './index.mojom-webui.js';
import {MessagePipe} from '//system_apps/message_pipe.js';
import {Message} from './message_types.js';
import {SearchConcept, SearchHandler} from './search.mojom-webui.js';
import {Content, ResponseStatus, Result} from './types.mojom-webui.js';

const helpApp = {
  handler: new PageHandlerRemote(),
};

// Set up a page handler to talk to the browser process.
PageHandlerFactory.getRemote().createPageHandler(
    helpApp.handler.$.bindNewPipeAndPassReceiver());

// Set up an index remote to talk to Local Search Service.
const indexRemote = Index.getRemote();

// Expose `indexRemote` on `window`, because it is accessed by a CrOS Tast test.
Object.assign(window, {indexRemote});

/**
 * Talks to the search handler. Use for updating the content for launcher
 * search.
 */
const searchHandlerRemote = SearchHandler.getRemote();

const GUEST_ORIGIN = 'chrome-untrusted://help-app';
const MAX_STRING_LEN = 9999;
const guestFrame = document.createElement('iframe');
guestFrame.src = `${GUEST_ORIGIN}${location.pathname}${location.search}`;
document.body.appendChild(guestFrame);

// Cached result whether Local Search Service is enabled.
const isLssEnabled =
    helpApp.handler.isLssEnabled().then(result => result.enabled);

// Cached result of whether Launcher Search is enabled.
const isLauncherSearchEnabled =
    helpApp.handler.isLauncherSearchEnabled().then(result => result.enabled);

/** Converts a string or object to url. */
function toUrl(url: string|object): Url {
  // TODO(b/279132899): Figure out why `url` is an empty object when it should
  // have been an empty string.
  if (url === '' || typeof (url) !== 'string') {
    return {url: ''};
  }
  return {url};
}

/** Converts string to String16. */
function toTruncatedString16(s: string): String16 {
  return stringToMojoString16(truncate(s));
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
  return helpApp.handler.openFeedbackDialog();
});

guestMessagePipe.registerHandler(
    Message.SHOW_ON_DEVICE_APP_CONTROLS,
    () => void helpApp.handler.showOnDeviceAppControls());

guestMessagePipe.registerHandler(Message.SHOW_PARENTAL_CONTROLS, () => void
  helpApp.handler.showParentalControls());

guestMessagePipe.registerHandler(
    Message.TRIGGER_WELCOME_TIP_CALL_TO_ACTION, (actionTypeId: number) => void
      helpApp.handler.triggerWelcomeTipCallToAction(actionTypeId));

guestMessagePipe.registerHandler(
    Message.ADD_OR_UPDATE_SEARCH_INDEX, async (data: SearchableItem[]) => {
      if (!(await isLssEnabled)) {
        return;
      }
      const dataToSend = data.map(searchableItem => {
        const contents: Content[] = [
          {
            id: TITLE_ID,
            content: toTruncatedString16(searchableItem.title),
            weight: 1.0,
          },
          {
            id: CATEGORY_ID,
            content: toTruncatedString16(searchableItem.mainCategoryName),
            weight: 0.1,
          },
        ];
        if (searchableItem.subcategoryNames) {
          for (let i = 0; i < searchableItem.subcategoryNames.length; ++i) {
            const subcategoryName = searchableItem.subcategoryNames[i]!;
            contents.push({
              id: SUBCATEGORY_ID + i,
              content: toTruncatedString16(subcategoryName),
              weight: 0.1,
            });
          }
        }
        // If there are subheadings, use those instead of the body.
        const subheadings = searchableItem.subheadings;
        if (subheadings) {
          for (let i = 0; i < subheadings.length; ++i) {
            const subheading = subheadings[i];
            if (!subheading) continue;
            contents.push({
              id: SUBHEADING_ID + i,
              content: toTruncatedString16(subheading),
              weight: 0.4,
            });
          }
        } else if (searchableItem.body) {
          contents.push({
            id: BODY_ID,
            content: toTruncatedString16(searchableItem.body),
            weight: 0.2,
          });
        }
        return {
          id: searchableItem.id,
          contents,
          locale: searchableItem.locale,
        };
      });
      return indexRemote.addOrUpdate(dataToSend);
    });

guestMessagePipe.registerHandler(Message.CLEAR_SEARCH_INDEX, async () => {
  if (!(await isLssEnabled)) {
    return;
  }
  return indexRemote.clearIndex();
});

guestMessagePipe.registerHandler(
  Message.FIND_IN_SEARCH_INDEX,
  async (dataFromApp: {query: string, maxResults: number}) => {
      if (!(await isLssEnabled)) {
        return {results: null};
      }
      const response = await indexRemote.find(
          toTruncatedString16(dataFromApp.query), dataFromApp.maxResults || 50);

      if (response.status !== ResponseStatus.kSuccess || !response.results) {
        return {results: null};
      }
      const searchResults: Result[] = (response.results);
      // Sort results by decreasing score.
      searchResults.sort((a, b) => b.score - a.score);
      const results: SearchResult[] = searchResults.map(result => {
        const titlePositions: Position[] = [];
        const bodyPositions: Position[] = [];
        // Id of the best subheading that appears in positions. We consider
        // the subheading containing the most match positions to be the best.
        // "" means no subheading positions found.
        let bestSubheadingId = '';
        /** Counts how many positions there are for each subheading id. */
        const subheadingPosCounts: Record<string, number> = {};
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
                newCount > (subheadingPosCounts[bestSubheadingId] ?? 0)) {
              bestSubheadingId = position.contentId;
            }
          }
        }
        // Use only the positions of the best subheading.
        const subheadingPositions: Position[] = [];
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
              Number(bestSubheadingId.substring(SUBHEADING_ID.length)) : null,
          subheadingPositions: bestSubheadingId ? subheadingPositions : null,
        };
      });
      return {results};
  },
);

guestMessagePipe.registerHandler(Message.CLOSE_BACKGROUND_PAGE, async () => {
  // TODO(b/186180962): Add background page and test that it closes when done.
  if (window.location.pathname !== '/background') {
    return;
  }
  window.close();
  return;
});

guestMessagePipe.registerHandler(
  Message.UPDATE_LAUNCHER_SEARCH_INDEX,
  async (message: LauncherSearchableItem[]) => {
      if (!(await isLauncherSearchEnabled)) {
        return;
      }

      const dataToSend: SearchConcept[] = message.map(
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
          (window as any).chrome.metricsPrivate
            .recordSparseValueWithPersistentHash(
              'Discover.LauncherSearch.InvalidConceptInUpdate', item.id);
        }
        return valid;
      });
      return searchHandlerRemote.update(dataFiltered);
  },
);

guestMessagePipe.registerHandler(Message.LAUNCH_MICROSOFT_365_SETUP, () => void
  helpApp.handler.launchMicrosoft365Setup());

guestMessagePipe.registerHandler(
    Message.MAYBE_SHOW_RELEASE_NOTES_NOTIFICATION, () => void
      helpApp.handler.maybeShowReleaseNotesNotification());

guestMessagePipe.registerHandler(Message.GET_DEVICE_INFO, async () => {
  return (await helpApp.handler.getDeviceInfo()).deviceInfo;
});

guestMessagePipe.registerHandler(
  Message.OPEN_URL_IN_BROWSER_AND_TRIGGER_INSTALL_DIALOG,
  (url: string | object) => {
    helpApp.handler.openUrlInBrowserAndTriggerInstallDialog(toUrl(url));
  },
);

/** Compare two positions by their start index. Use for sorting. */
function compareByStart(a: Position, b: Position): number {
  return a.start - b.start;
}

/**
 * Limits the maximum length of the input string. Converts non-strings into
 * empty string.
 *
 * @param s Probably a string, but might not be.
 */
function truncate(s: any): string {
  if (typeof s !== 'string') {
    return '';
  }
  if (s.length <= MAX_STRING_LEN) {
    return s;
  }
  return s.substring(0, MAX_STRING_LEN);
}

export const TEST_ONLY = {guestMessagePipe};
