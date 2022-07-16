// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export {ensureLazyLoaded, listenForPrivilegedLinkClicks} from './app.js';
export {BrowserService} from './browser_service.js';
export {HistoryPageViewHistogram, SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram} from './constants.js';
export {BrowserProxyImpl} from './history_clusters/browser_proxy.js';
export {PageCallbackRouter, PageHandlerRemote} from './history_clusters/history_clusters.mojom-webui.js';
export {MetricsProxyImpl} from './history_clusters/metrics_proxy.js';
