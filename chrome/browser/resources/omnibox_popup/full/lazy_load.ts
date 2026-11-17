// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file holds dependencies that are deliberately excluded
 * from the main omnibox_popup_full.js module, to force them to load after
 * core elements of the full WebUI omnibox popup. This improves initial load
 * and rendering performance by deferring non-core elements that are hidden on
 * initial render.
 */

// LINT.IfChange(LazyLoadedModules)
import 'chrome://omnibox-popup.top-chrome/shared/omnibox_popup_contextual_entrypoint.js';

// LINT.ThenChange(
//     //chrome/browser/resources/omnibox_popup/full/omnibox_popup_searchbox.ts:LazyLoadedModules,
//     //chrome/test/data/webui/omnibox_popup/lazy_load_test.ts:LazyLoadedModules
// )

export {OmniboxPopupContextualEntrypointElement} from 'chrome://omnibox-popup.top-chrome/shared/omnibox_popup_contextual_entrypoint.js';
export {OmniboxPopupContextualEntrypointButtonElement} from 'chrome://omnibox-popup.top-chrome/shared/omnibox_popup_contextual_entrypoint_button.js';
