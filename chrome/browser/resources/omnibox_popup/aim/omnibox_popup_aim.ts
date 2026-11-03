// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://omnibox-popup.top-chrome/shared/profile_load_time_data.js';
import './aim_app.js';

export {AimButtonMode} from '//resources/cr_components/composebox/composebox_aim_button.js';
export {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
export {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
export {OmniboxAimAppElement} from './aim_app.js';
export {OmniboxComposeboxElement} from './omnibox_composebox.js';
export {browserProxyFactory as aimBrowserProxyFactory, PageHandlerRemote as OmniboxPopupAimPageHandlerRemote, PageRemote as OmniboxPopupAimPageRemote} from './omnibox_popup_aim.mojom-webui.js';
