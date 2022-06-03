// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OmniboxPageCallbackRouter, OmniboxPageHandler, OmniboxPageHandlerRemote} from '/chrome/browser/ui/webui/omnibox/omnibox.mojom-webui.js';
import {AutocompleteMatchListElement} from 'chrome://resources/cr_components/omnibox/cr_autocomplete_match_list.js';

/**
 * Javascript proof-of-concept for omnibox_popup.html, served from
 * chrome://omnibox/omnibox_popup.html. This is used for the experimental
 * WebUI version of the omnibox popup.
 */

document.addEventListener('DOMContentLoaded', () => {
  /** @private {!OmniboxPageCallbackRouter} */
  const callbackRouter = new OmniboxPageCallbackRouter;

  // Basically a Hello World proof of concept that writes the Autocomplete
  // responses to the whole document.
  callbackRouter.handleNewAutocompleteResponse.addListener(
      (response, isPageController) => {
        // Ignore debug controller and empty results.
        if (!isPageController && response.combinedResults.length > 0) {
          /** @private {!AutocompleteMatchListElement} */
          const popup = /** @type {!AutocompleteMatchListElement} */ (
              document.querySelector('cr-autocomplete-match-list'));

          popup.updateMatches(response.combinedResults);
        }
      });

  /** @private {!OmniboxPageHandlerRemote} */
  const handler = OmniboxPageHandler.getRemote();
  handler.setClientPage(callbackRouter.$.bindNewPipeAndPassRemote());
});
