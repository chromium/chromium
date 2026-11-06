// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://omnibox-popup.top-chrome/shared/profile_load_time_data.js';
import './app.js';

export {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
export {browserProxyFactory as omniboxPopupBrowserProxyFactory, PageHandlerRemote as OmniboxPopupPageHandlerRemote, PageRemote as OmniboxPopupPageRemote} from 'chrome://omnibox-popup.top-chrome/shared/omnibox_popup.mojom-webui.js';
export {OmniboxPopupContextualEntrypointElement} from 'chrome://omnibox-popup.top-chrome/shared/omnibox_popup_contextual_entrypoint.js';
export {OmniboxPopupContextualEntrypointButtonElement} from 'chrome://omnibox-popup.top-chrome/shared/omnibox_popup_contextual_entrypoint_button.js';
export {OmniboxPopupAppElement} from './app.js';
